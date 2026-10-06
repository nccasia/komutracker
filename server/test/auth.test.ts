import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { Request } from 'express';
import { DataSource } from 'typeorm';
import { createAuth } from '../auth';
import { AppConfig } from '../config';
import { UserRecord } from '../entities';

const config: AppConfig = {
    port: 5678,
    host: '127.0.0.1',
    databaseUrl: '',
    databaseSsl: false,
    databasePoolSize: 1,
    authToken: '',
    reportApiKey: '',
    mergeWindowSeconds: 370,
    oauth: {
        clientId: 'client-id',
        clientSecret: 'client-secret',
        tokenUrl: 'https://oauth.example/token',
        userInfoUrl: 'https://oauth.example/userinfo',
        redirectUri: 'https://tracker.example/api/0/auth/callback',
        legacyRedirectUri: '',
    },
};

function request(deviceId: string, token: string): Request {
    return {
        header(name: string) {
            if (name.toLowerCase() === 'device-id') return deviceId;
            if (name.toLowerCase() === 'authorization') return `Bearer ${token}`;
            return undefined;
        },
    } as Request;
}

describe('authentication logout', () => {
    it('revokes the persisted token so a new login poll cannot reuse it', async () => {
        const user = {
            id: 'user-1',
            deviceId: 'device-1',
            mezonId: 'mezon-1',
            name: 'Test User',
            email: 'test@example.com',
            authToken: 'old-token',
            createdAt: new Date(),
            updatedAt: new Date(),
            lastSeenAt: new Date(),
        } satisfies UserRecord;

        const repository = {
            async findOneBy(where: Partial<UserRecord>) {
                return where.deviceId === user.deviceId ? user : null;
            },
            async save(value: UserRecord) {
                return value;
            },
            async update(id: string, value: Partial<UserRecord>) {
                assert.equal(id, user.id);
                Object.assign(user, value);
            },
        };
        const dataSource = {
            getRepository: () => repository,
        } as unknown as DataSource;
        const auth = createAuth(dataSource, config);

        assert.equal(await auth.poll(user.deviceId), 'old-token');
        await auth.logout(user.id);
        assert.equal(user.authToken, null);
        assert.equal(await auth.poll(user.deviceId), null);
    });
});

describe('single active client', () => {
    it('replaces the previous device and token and invalidates its cached session', async () => {
        const now = new Date();
        const oldUser: UserRecord = {
            id: 'user-1', deviceId: 'old-device', mezonId: 'mezon-1',
            name: 'Test User', email: 'old@example.com', authToken: 'old-token',
            createdAt: now, updatedAt: now, lastSeenAt: now,
        };
        const pendingUser: UserRecord = {
            id: 'pending-1', deviceId: 'new-device', mezonId: null,
            name: 'Pending', email: 'pending@local', authToken: null,
            createdAt: now, updatedAt: now, lastSeenAt: now,
        };
        const users = [oldUser, pendingUser];
        const repository = {
            async findOneBy(where: Partial<UserRecord>) {
                return users.find((user) => user.deviceId === where.deviceId) || null;
            },
            async save(value: UserRecord) { users.push(value); return value; },
            async update(id: string, value: Partial<UserRecord>) {
                const user = users.find((item) => item.id === id);
                if (user) Object.assign(user, value);
            },
        };
        const manager = {
            async findOne(_entity: unknown, options: { where: Partial<UserRecord> }) {
                return users.find((user) => user.mezonId === options.where.mezonId) || null;
            },
            async delete(_entity: unknown, id: string) {
                const index = users.findIndex((user) => user.id === id);
                if (index >= 0) users.splice(index, 1);
            },
            async update(_entity: unknown, id: string, value: Partial<UserRecord>) {
                await repository.update(id, value);
            },
        };
        const dataSource = {
            getRepository: () => repository,
            async transaction<T>(run: (value: typeof manager) => Promise<T>) { return run(manager); },
        } as unknown as DataSource;
        const originalFetch = globalThis.fetch;
        globalThis.fetch = async (input) => {
            const url = String(input);
            if (url === config.oauth.tokenUrl) {
                return new Response(JSON.stringify({ access_token: 'new-token' }), { status: 200 });
            }
            return new Response(JSON.stringify({
                user_id: 'mezon-1', name: 'Test User', email: 'new@example.com',
            }), { status: 200 });
        };

        try {
            const auth = createAuth(dataSource, config);
            assert.equal((await auth.session(request('old-device', 'old-token')))?.id, oldUser.id);

            await auth.callback(
                new URLSearchParams({ state: 'new-device', code: 'oauth-code' }),
                config.oauth.redirectUri,
            );

            assert.equal(await auth.session(request('old-device', 'old-token')), null);
            assert.equal((await auth.session(request('new-device', 'new-token')))?.id, oldUser.id);
            assert.equal(oldUser.deviceId, 'new-device');
            assert.equal(oldUser.authToken, 'new-token');
            assert.equal(users.includes(pendingUser), false);
        } finally {
            globalThis.fetch = originalFetch;
        }
    });
});
