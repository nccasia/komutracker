import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { DataSource } from 'typeorm';
import { createAuth } from '../auth';
import { AppConfig } from '../config';
import { UserRecord } from '../entities';

const config = { authToken: '' } as AppConfig;

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
