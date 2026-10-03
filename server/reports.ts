import { DataSource } from 'typeorm';
import { assertRangeWithinRetention } from './retention';

export interface UserDayReport {
    userId: string;
    email: string;
    mezonId: string | null;
    name: string;
    active_time: string;
}

interface UserDayReportRow extends Omit<UserDayReport, 'active_time'> {
    active_seconds: number | string;
}

export interface EventDetail {
    id: string;
    status: 'afk' | 'not-afk';
    startAt: Date;
    endAt: Date;
}

export class UserNotFoundError extends Error {
    statusCode = 404;

    constructor() {
        super('User not found');
    }
}

function dayRange(day: string): [Date, Date] {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(day)) throw new Error('day must use YYYY-MM-DD');
    const start = new Date(`${day}T00:00:00+07:00`);
    if (Number.isNaN(start.getTime())) throw new Error('Invalid day');
    return [start, new Date(start.getTime() + 24 * 60 * 60 * 1000)];
}

function timeRange(startValue: string, endValue: string): [Date, Date] {
    const start = new Date(startValue);
    const end = new Date(endValue);
    if (Number.isNaN(start.getTime()) || Number.isNaN(end.getTime()) || end <= start) {
        throw new Error('start and end must be valid ISO timestamps with end after start');
    }
    return [start, end];
}

function formatDuration(seconds: number | string): string {
    const total = Math.max(0, Math.round(Number(seconds) || 0));
    const hours = Math.floor(total / 3600);
    const minutes = Math.floor((total % 3600) / 60);
    const remainingSeconds = total % 60;
    return [hours, minutes, remainingSeconds].map((value) => String(value).padStart(2, '0')).join(':');
}

export function createReports(dataSource: DataSource, now: () => Date = () => new Date()) {
    return {
        async usersForDay(day: string): Promise<UserDayReport[]> {
            const [start, end] = dayRange(day);
            assertRangeWithinRetention(start, end, now());
            const rows = await dataSource.query<UserDayReportRow[]>(`
                SELECT
                    u.id AS "userId",
                    u.email,
                    u.mezon_id AS "mezonId",
                    u.name,
                    COALESCE(SUM(
                        CASE WHEN e.status = 'not-afk' THEN
                            EXTRACT(EPOCH FROM (
                                LEAST(e.end_at, $2::timestamptz) -
                                GREATEST(e.start_at, $1::timestamptz)
                            ))
                        ELSE 0
                        END
                    ), 0)::double precision AS active_seconds
                FROM users u
                LEFT JOIN afk_events e
                    ON e.user_id = u.id
                    AND e.start_at < $2::timestamptz
                    AND e.end_at > $1::timestamptz
                GROUP BY u.id, u.email, u.mezon_id, u.name
                ORDER BY u.name ASC, u.email ASC
            `, [start, end]);
            return rows.map((row) => ({
                ...row,
                active_time: formatDuration(row.active_seconds),
            }));
        },

        async eventsForHostname(hostname: string, startValue: string, endValue: string): Promise<EventDetail[]> {
            const normalizedHostname = hostname.trim().toLowerCase();
            if (!/^[a-z0-9][a-z0-9._-]{0,127}$/.test(normalizedHostname)) throw new Error('Invalid hostname');
            const [start, end] = timeRange(startValue, endValue);
            assertRangeWithinRetention(start, end, now());
            const users = await dataSource.query<{ id: string }[]>(`
                SELECT id
                FROM users
                WHERE lower(split_part(email, '@', 1)) = $1
                LIMIT 1
            `, [normalizedHostname]);
            if (!users[0]) throw new UserNotFoundError();

            return dataSource.query<EventDetail[]>(`
                SELECT
                    id::text AS id,
                    status,
                    start_at AS "startAt",
                    end_at AS "endAt"
                FROM afk_events
                WHERE user_id = $1::uuid
                  AND start_at < $3::timestamptz
                  AND end_at > $2::timestamptz
                ORDER BY start_at ASC, id ASC
            `, [users[0].id, start, end]);
        },
    };
}
