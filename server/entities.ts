import { EntitySchema } from 'typeorm';

export interface UserRecord {
    id: string;
    deviceId: string;
    mezonId: string | null;
    name: string;
    email: string;
    authToken: string | null;
    createdAt: Date;
    updatedAt: Date;
    lastSeenAt: Date;
}

export interface AfkEventRecord {
    id: string;
    userId: string;
    status: 'afk' | 'not-afk';
    startAt: Date;
    endAt: Date;
    createdAt: Date;
}

export const UserEntity = new EntitySchema<UserRecord>({
    name: 'User',
    tableName: 'users',
    columns: {
        id: { type: 'uuid', primary: true, generated: 'uuid' },
        deviceId: { name: 'device_id', type: 'varchar', length: 128, unique: true },
        mezonId: { name: 'mezon_id', type: 'varchar', length: 255, unique: true, nullable: true },
        name: { type: 'varchar', length: 255 },
        email: { type: 'varchar', length: 320 },
        authToken: { name: 'auth_token', type: 'varchar', length: 2048, nullable: true },
        createdAt: { name: 'created_at', type: 'timestamptz', createDate: true },
        updatedAt: { name: 'updated_at', type: 'timestamptz', updateDate: true },
        lastSeenAt: { name: 'last_seen_at', type: 'timestamptz', updateDate: true },
    },
});

export const AfkEventEntity = new EntitySchema<AfkEventRecord>({
    name: 'AfkEvent',
    tableName: 'afk_events',
    columns: {
        id: { type: 'bigint', primary: true, generated: 'increment' },
        userId: { name: 'user_id', type: 'uuid' },
        status: { type: 'varchar', length: 16 },
        startAt: { name: 'start_at', type: 'timestamptz' },
        endAt: { name: 'end_at', type: 'timestamptz' },
        createdAt: { name: 'created_at', type: 'timestamptz', createDate: true },
    },
    indices: [
        { name: 'idx_afk_events_user_start_end', columns: ['userId', 'startAt', 'endAt'] },
        { name: 'idx_afk_events_start_end_user', columns: ['startAt', 'endAt', 'userId'] },
    ],
});
