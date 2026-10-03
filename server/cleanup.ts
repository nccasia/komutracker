import { DataSource } from 'typeorm';
import { CronJob } from 'cron';
import { AfkEventEntity } from './entities';
import { retentionWindow } from './retention';

export const ACTIVITY_CLEANUP_SCHEDULE = '0 1 * * *';
export const ACTIVITY_CLEANUP_TIME_ZONE = 'Asia/Ho_Chi_Minh';

export async function cleanupExpiredActivity(dataSource: DataSource, now = new Date()): Promise<number> {
    const cutoff = retentionWindow(now).start;
    const result = await dataSource.getRepository(AfkEventEntity)
        .createQueryBuilder()
        .delete()
        .where('end_at < :cutoff', { cutoff })
        .execute();
    return result.affected || 0;
}

export function startActivityCleanupJob(dataSource: DataSource): () => void {
    const job = CronJob.from({
        cronTime: ACTIVITY_CLEANUP_SCHEDULE,
        timeZone: ACTIVITY_CLEANUP_TIME_ZONE,
        start: true,
        waitForCompletion: true,
        onTick: async () => {
            try {
                const deleted = await cleanupExpiredActivity(dataSource);
                console.log(`activity cleanup removed ${deleted} expired event(s)`);
            } catch (error) {
                console.error('activity cleanup failed:', error instanceof Error ? error.message : error);
            }
        },
    });

    return () => job.stop();
}
