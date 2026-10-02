import 'reflect-metadata';
import path from 'node:path';
import { DataSource } from 'typeorm';
import { AppConfig } from './config';
import { AfkEventEntity, UserEntity } from './entities';

export function createDataSource(config: AppConfig): DataSource {
    if (!config.databaseUrl) throw new Error('DATABASE_URL is required');
    return new DataSource({
        type: 'postgres',
        url: config.databaseUrl,
        ssl: config.databaseSsl ? { rejectUnauthorized: false } : false,
        synchronize: false,
        migrationsRun: false,
        migrations: [path.join(__dirname, 'migrations/*.{js,ts}')],
        entities: [UserEntity, AfkEventEntity],
        extra: {
            max: config.databasePoolSize,
            connectionTimeoutMillis: 5000,
            idleTimeoutMillis: 30000,
        },
    });
}
