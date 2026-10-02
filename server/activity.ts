import { DataSource } from 'typeorm';
import { AfkEventEntity, AfkEventRecord } from './entities';
import { AppConfig } from './config';

export interface AfkHeartbeat {
    timestamp: string;
    duration?: number;
    data: { status?: string };
}

export function createActivity(dataSource: DataSource, config: AppConfig) {
    const events = dataSource.getRepository(AfkEventEntity);

    return {
        async heartbeat(userId: string, value: AfkHeartbeat): Promise<void> {
            if (!value || !value.timestamp || !value.data) throw new Error('Invalid AFK event');
            const status = value.data.status;
            const duration = Number(value.duration || 0);
            const startAt = new Date(value.timestamp);
            if (status !== 'afk' && status !== 'not-afk') {
                throw new Error('Invalid AFK event');
            }
            if (!Number.isFinite(duration) || duration < 0 || Number.isNaN(startAt.getTime())) throw new Error('Invalid AFK event');
            const endAt = new Date(startAt.getTime() + duration * 1000);
            const mergeWindowMs = Math.max(0, config.mergeWindowSeconds) * 1000;

            await dataSource.transaction(async (manager) => {
                const repository = manager.getRepository<AfkEventRecord>(AfkEventEntity);
                const latest = await repository.createQueryBuilder('event')
                    .where('event.user_id = :userId', { userId })
                    .orderBy('event.start_at', 'DESC')
                    .limit(1)
                    .setLock('pessimistic_write')
                    .getOne();

                const gapMs = latest ? startAt.getTime() - latest.endAt.getTime() : Number.POSITIVE_INFINITY;
                if (latest && latest.status === status && Math.abs(gapMs) <= mergeWindowMs) {
                    await repository.update(latest.id, {
                        startAt: startAt < latest.startAt ? startAt : latest.startAt,
                        endAt: endAt > latest.endAt ? endAt : latest.endAt,
                    });
                    return;
                }
                await repository.insert({ userId, status, startAt, endAt });
            });
        },
    };
}
