import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { DataSource } from 'typeorm';
import { createActivity, intervalGapMs, mergeWindowMilliseconds } from '../activity';
import { AppConfig } from '../config';
import { AfkEventRecord } from '../entities';

const at = (seconds: number) => new Date(seconds * 1000);

describe('intervalGapMs', () => {
    it('treats a growing AFK heartbeat as overlapping after the merge window', () => {
        const storedStart = at(0);
        const storedEnd = at(370);
        const heartbeatStart = at(0);
        const heartbeatEnd = at(380);

        assert.equal(intervalGapMs(storedStart, storedEnd, heartbeatStart, heartbeatEnd), 0);
    });

    it('returns the distance between non-overlapping heartbeat intervals', () => {
        assert.equal(intervalGapMs(at(0), at(10), at(20), at(30)), 10_000);
        assert.equal(intervalGapMs(at(20), at(30), at(0), at(10)), 10_000);
    });

    it('returns zero for partial and contained overlaps', () => {
        assert.equal(intervalGapMs(at(0), at(20), at(10), at(30)), 0);
        assert.equal(intervalGapMs(at(0), at(30), at(10), at(20)), 0);
    });
});

describe('mergeWindowMilliseconds', () => {
    it('rejects invalid values instead of silently disabling every merge', () => {
        assert.throws(() => mergeWindowMilliseconds(Number.NaN), /MERGE_WINDOW_SECONDS/);
        assert.throws(() => mergeWindowMilliseconds(Number.POSITIVE_INFINITY), /MERGE_WINDOW_SECONDS/);
        assert.throws(() => mergeWindowMilliseconds(-1), /MERGE_WINDOW_SECONDS/);
        assert.equal(mergeWindowMilliseconds(370), 370_000);
    });
});

describe('activity heartbeat merging', () => {
    it('uses the greatest end time as latest when a transition marker starts a few milliseconds later', async () => {
        const start = new Date('2026-10-04T02:53:00.000Z');
        const rows: AfkEventRecord[] = [
            {
                id: '1', userId: 'user-1', status: 'afk', startAt: start,
                endAt: new Date(start.getTime() + 100_000), createdAt: start,
            },
            {
                id: '2', userId: 'user-1', status: 'not-afk',
                startAt: new Date(start.getTime() + 2),
                endAt: new Date(start.getTime() + 2), createdAt: start,
            },
        ];
        const order: Array<{ column: string; direction: 'ASC' | 'DESC' }> = [];
        const actions: string[] = [];
        const updates: Array<{ id: string; value: Partial<AfkEventRecord> }> = [];
        const inserts: Partial<AfkEventRecord>[] = [];
        const builder = {
            where() { return this; },
            orderBy(column: string, direction: 'ASC' | 'DESC') {
                order.length = 0;
                order.push({ column, direction });
                return this;
            },
            addOrderBy(column: string, direction: 'ASC' | 'DESC') {
                order.push({ column, direction });
                return this;
            },
            limit() { return this; },
            setLock() { return this; },
            async getOne() {
                actions.push('select');
                const property: Record<string, keyof AfkEventRecord> = {
                    'event.end_at': 'endAt',
                    'event.start_at': 'startAt',
                    'event.id': 'id',
                };
                return [...rows].sort((left, right) => {
                    for (const item of order) {
                        const leftValue = left[property[item.column]];
                        const rightValue = right[property[item.column]];
                        const comparison = leftValue instanceof Date && rightValue instanceof Date
                            ? leftValue.getTime() - rightValue.getTime()
                            : String(leftValue).localeCompare(String(rightValue), undefined, { numeric: true });
                        if (comparison) return item.direction === 'DESC' ? -comparison : comparison;
                    }
                    return 0;
                })[0];
            },
        };
        const repository = {
            createQueryBuilder: () => builder,
            async update(id: string, value: Partial<AfkEventRecord>) { updates.push({ id, value }); },
            async insert(value: Partial<AfkEventRecord>) { inserts.push(value); },
        };
        const manager = {
            async query(sql: string, values: unknown[]) {
                actions.push('advisory-lock');
                assert.match(sql, /pg_advisory_xact_lock/);
                assert.deepEqual(values, ['user-1']);
            },
            getRepository: () => repository,
        };
        const dataSource = {
            async transaction<T>(run: (value: typeof manager) => Promise<T>) { return run(manager); },
        } as unknown as DataSource;
        const config = { mergeWindowSeconds: 370 } as AppConfig;

        await createActivity(dataSource, config).heartbeat('user-1', {
            timestamp: start.toISOString(),
            duration: 110,
            data: { status: 'afk' },
        });

        assert.deepEqual(order, [
            { column: 'event.end_at', direction: 'DESC' },
            { column: 'event.start_at', direction: 'DESC' },
            { column: 'event.id', direction: 'DESC' },
        ]);
        assert.deepEqual(actions, ['advisory-lock', 'select']);
        assert.equal(updates.length, 1);
        assert.equal(updates[0].id, '1');
        assert.equal(updates[0].value.endAt?.getTime(), start.getTime() + 110_000);
        assert.equal(inserts.length, 0);
    });
});
