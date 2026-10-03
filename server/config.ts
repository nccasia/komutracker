import 'dotenv/config';

export interface OAuthConfig {
    clientId: string;
    clientSecret: string;
    tokenUrl: string;
    userInfoUrl: string;
    redirectUri: string;
    legacyRedirectUri?: string;
}

export interface AppConfig {
    port: number;
    host: string;
    databaseUrl: string;
    databaseSsl: boolean;
    databasePoolSize: number;
    authToken: string;
    reportApiKey: string;
    mergeWindowSeconds: number;
    oauth: OAuthConfig;
}

export function loadConfig(): AppConfig {
    return {
        port: Number(process.env.PORT || 5678),
        host: process.env.HOST || '127.0.0.1',
        databaseUrl: process.env.DATABASE_URL || '',
        databaseSsl: process.env.DATABASE_SSL === 'true',
        databasePoolSize: Number(process.env.DATABASE_POOL_SIZE || 20),
        authToken: process.env.AUTH_TOKEN || '',
        reportApiKey: process.env.REPORT_API_KEY || 'secret-key',
        mergeWindowSeconds: Number(process.env.MERGE_WINDOW_SECONDS || 370),
        oauth: {
            clientId: process.env.MEZON_CLIENT_ID || process.env.OAUTH_CLIENT_ID || '',
            clientSecret: process.env.MEZON_CLIENT_SECRET || process.env.OAUTH_CLIENT_SECRET || '',
            tokenUrl: process.env.MEZON_TOKEN_URL || 'https://oauth2.mezon.ai/oauth2/token',
            userInfoUrl: process.env.MEZON_USERINFO_URL || 'https://oauth2.mezon.ai/userinfo',
            redirectUri: process.env.MEZON_REDIRECT_URI || process.env.OAUTH_REDIRECT_URI || '',
            legacyRedirectUri: process.env.MEZON_LEGACY_REDIRECT_URI
                || 'https://tracker-api.komu.vn/api/0/auth/callback',
        },
    };
}
