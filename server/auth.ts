import crypto from 'node:crypto';
import { Request } from 'express';
import { DataSource } from 'typeorm';
import { AppConfig } from './config';
import { TtlCache } from './cache';
import { UserEntity, UserRecord } from './entities';

interface OAuthTokens {
    access_token: string;
}

export interface UserProfile {
    mezonId: string;
    name: string;
    email: string;
}

export interface SessionUser {
    id: string;
    name: string;
    email: string;
    deviceId: string;
    authToken: string | null;
}

function bearer(request: Request): string | null {
    const value = request.header('authorization') || '';
    const match = value.match(/^Bearer\s+(.+)$/i);
    return match ? match[1] : null;
}

function sameToken(expected: string | null, actual: string | null): boolean {
    if (!expected || !actual) return false;
    const expectedBuffer = Buffer.from(expected);
    const actualBuffer = Buffer.from(actual);
    return expectedBuffer.length === actualBuffer.length && crypto.timingSafeEqual(expectedBuffer, actualBuffer);
}

function profileFrom(value: Record<string, unknown>): UserProfile {
    const email = String(value.email || value.preferred_username || '');
    return {
        mezonId: String(value.user_id || value.sub || value.id || email),
        name: String(value.name || value.username || email || 'User'),
        email: email || 'unknown@local',
    };
}

async function exchangeCode(config: AppConfig, code: string, state: string): Promise<OAuthTokens> {
    if (!config.oauth.clientId || !config.oauth.clientSecret || !config.oauth.redirectUri) {
        throw new Error('Mezon OAuth configuration is incomplete');
    }
    const form = new URLSearchParams({
        grant_type: 'authorization_code',
        code,
        state,
        client_id: config.oauth.clientId,
        client_secret: config.oauth.clientSecret,
        redirect_uri: config.oauth.redirectUri,
    });
    const response = await fetch(config.oauth.tokenUrl, {
        method: 'POST',
        headers: { 'content-type': 'application/x-www-form-urlencoded' },
        body: form,
    });
    if (!response.ok) throw new Error(`Mezon token request failed with HTTP ${response.status}`);
    return response.json() as Promise<OAuthTokens>;
}

async function fetchProfile(config: AppConfig, tokens: OAuthTokens): Promise<UserProfile> {
    let response = await fetch(config.oauth.userInfoUrl, {
        headers: { authorization: `Bearer ${tokens.access_token}` },
    });
    if (!response.ok) {
        const form = new URLSearchParams({
            access_token: tokens.access_token,
            client_id: config.oauth.clientId,
            client_secret: config.oauth.clientSecret,
        });
        response = await fetch(config.oauth.userInfoUrl, {
            method: 'POST',
            headers: { 'content-type': 'application/x-www-form-urlencoded' },
            body: form,
        });
    }
    if (!response.ok) throw new Error(`Mezon profile request failed with HTTP ${response.status}`);
    return profileFrom(await response.json() as Record<string, unknown>);
}

export function createAuth(dataSource: DataSource, config: AppConfig) {
    const users = dataSource.getRepository<UserRecord>(UserEntity);
    const cache = new TtlCache<SessionUser>(60000, 10000);

    async function ensureUser(deviceId: string): Promise<UserRecord> {
        if (!deviceId) throw new Error('device_id is required');
        let user = await users.findOneBy({ deviceId });
        if (!user) {
            user = await users.save({
                deviceId,
                name: `device-${deviceId.slice(0, 12)}`,
                email: `${deviceId}@local`,
                mezonId: null,
                authToken: null,
                lastSeenAt: new Date(),
            });
        } else {
            await users.update(user.id, { lastSeenAt: new Date() });
        }
        return user;
    }

    return {
        // TODO: Remove this legacy ensureUser once deprecated clients call /api/0/auth before /api/0/auth/me.
        ensureUser,

        async poll(deviceId: string): Promise<string | null> {
            const user = await ensureUser(deviceId);
            return config.authToken || user.authToken || null;
        },

        async callback(query: URLSearchParams): Promise<UserProfile> {
            const deviceId = query.get('state');
            const code = query.get('code');
            if (!deviceId || deviceId.length > 128 || !code) throw new Error('Mezon callback requires device state and code');
            const user = await users.findOneBy({ deviceId });
            if (!user) throw new Error('Unknown device login request');
            const tokens = await exchangeCode(config, code, deviceId);
            const profile = await fetchProfile(config, tokens);

            await dataSource.transaction(async (manager) => {
                const existing = await manager.findOne(UserEntity, { where: { email: profile.email } });
                const target = existing || user;
                if (existing && existing.id !== user.id) await manager.delete(UserEntity, user.id);
                await manager.update(UserEntity, target.id, {
                    deviceId,
                    ...profile,
                    authToken: tokens.access_token,
                    lastSeenAt: new Date(),
                });
            });
            cache.clear();
            return profile;
        },

        async session(request: Request): Promise<SessionUser | null> {
            const deviceId = request.header('device-id');
            const token = bearer(request);
            if (!deviceId || !token) return null;
            const cached = cache.get(deviceId);
            if (cached && (config.authToken ? sameToken(config.authToken, token) : sameToken(cached.authToken, token))) return cached;
            const user = await users.findOneBy({ deviceId });
            if (!user) return null;
            if (config.authToken ? !sameToken(config.authToken, token) : !sameToken(user.authToken, token)) return null;
            const value: SessionUser = { id: user.id, name: user.name, email: user.email, deviceId, authToken: user.authToken };
            cache.set(deviceId, value);
            return value;
        },

        logout(): void {
            cache.clear();
        },
    };
}
