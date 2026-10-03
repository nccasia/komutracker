import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { selectOAuthRedirectUri } from '../app';
import { AppConfig } from '../config';

const config: AppConfig = {
    port: 5678,
    host: '127.0.0.1',
    databaseUrl: '',
    databaseSsl: false,
    databasePoolSize: 1,
    authToken: '',
    reportApiKey: 'report-key',
    mergeWindowSeconds: 370,
    oauth: {
        clientId: 'client-id',
        clientSecret: 'client-secret',
        tokenUrl: 'https://oauth.example/token',
        userInfoUrl: 'https://oauth.example/userinfo',
        redirectUri: 'https://tracker.komu.vn/api/0/auth/callback',
        legacyRedirectUri: 'https://tracker-api.komu.vn/api/0/auth/callback',
    },
};

describe('selectOAuthRedirectUri', () => {
    it('uses the legacy redirect only for the allowlisted legacy host', () => {
        assert.equal(
            selectOAuthRedirectUri(config, 'tracker-api.komu.vn'),
            'https://tracker-api.komu.vn/api/0/auth/callback',
        );
        assert.equal(
            selectOAuthRedirectUri(config, 'TRACKER-API.KOMU.VN.'),
            'https://tracker-api.komu.vn/api/0/auth/callback',
        );
    });

    it('uses the configured current redirect for the current and unknown hosts', () => {
        assert.equal(
            selectOAuthRedirectUri(config, 'tracker.komu.vn'),
            'https://tracker.komu.vn/api/0/auth/callback',
        );
        assert.equal(
            selectOAuthRedirectUri(config, 'attacker.example'),
            'https://tracker.komu.vn/api/0/auth/callback',
        );
    });
});
