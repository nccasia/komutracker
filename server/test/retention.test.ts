import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { validateCronExpression } from 'cron';
import { DataSource } from 'typeorm';
import {
    ACTIVITY_CLEANUP_SCHEDULE,
    ACTIVITY_CLEANUP_TIME_ZONE,
    cleanupExpiredActivity,
} from '../cleanup';
import { assertRangeWithinRetention, retentionWindow } from '../retention';

describe('retentionWindow', () => {
    const now = new Date('2026-10-03T03:00:00.000Z'); // 10:00 GMT+7

    it('keeps 90 calendar days including today in GMT+7', () => {
        const window = retentionWindow(now);
        assert.equal(window.start.toISOString(), '2026-07-05T17:00:00.000Z');
        assert.equal(window.end.toISOString(), '2026-10-03T17:00:00.000Z');
        assert.equal((window.end.getTime() - window.start.getTime()) / 86_400_000, 90);
    });

    it('accepts the boundaries and rejects older or future ranges', () => {
        const window = retentionWindow(now);
        assert.doesNotThrow(() => assertRangeWithinRetention(window.start, window.end, now));
        assert.throws(
            () => assertRangeWithinRetention(new Date(window.start.getTime() - 1), window.end, now),
            /last 90 days/,
        );
        assert.throws(
            () => assertRangeWithinRetention(window.start, new Date(window.end.getTime() + 1), now),
            /last 90 days/,
        );
    });
});

describe('activity cleanup schedule', () => {
    it('uses a valid daily 01:00 cron expression in the GMT+7 timezone', () => {
        assert.equal(validateCronExpression(ACTIVITY_CLEANUP_SCHEDULE).valid, true);
        assert.equal(ACTIVITY_CLEANUP_SCHEDULE, '0 1 * * *');
        assert.equal(ACTIVITY_CLEANUP_TIME_ZONE, 'Asia/Ho_Chi_Minh');
    });
});

describe('cleanupExpiredActivity', () => {
    it('deletes events ending before the retention boundary', async () => {
        let whereClause = '';
        let whereParameters: Record<string, unknown> = {};
        const queryBuilder = {
            delete() { return this; },
            where(clause: string, parameters: Record<string, unknown>) {
                whereClause = clause;
                whereParameters = parameters;
                return this;
            },
            async execute() { return { affected: 4 }; },
        };
        const dataSource = {
            getRepository() {
                return { createQueryBuilder: () => queryBuilder };
            },
        } as unknown as DataSource;

        const deleted = await cleanupExpiredActivity(dataSource, new Date('2026-10-03T03:00:00.000Z'));

        assert.equal(deleted, 4);
        assert.equal(whereClause, 'end_at < :cutoff');
        assert.equal((whereParameters.cutoff as Date).toISOString(), '2026-07-05T17:00:00.000Z');
    });
});
