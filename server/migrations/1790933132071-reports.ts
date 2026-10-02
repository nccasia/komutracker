import { MigrationInterface, QueryRunner } from "typeorm";

export class Reports1790933132071 implements MigrationInterface {
    name = 'Reports1790933132071'

    public async up(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`DROP INDEX "public"."idx_afk_events_user_start"`);
        await queryRunner.query(`CREATE INDEX "idx_afk_events_user_start_end" ON "afk_events" ("user_id", "start_at", "end_at") `);
        await queryRunner.query(`CREATE INDEX "idx_afk_events_start_end_user" ON "afk_events" ("start_at", "end_at", "user_id") `);
    }

    public async down(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`DROP INDEX "public"."idx_afk_events_start_end_user"`);
        await queryRunner.query(`DROP INDEX "public"."idx_afk_events_user_start_end"`);
        await queryRunner.query(`CREATE INDEX "idx_afk_events_user_start" ON "afk_events" ("start_at", "user_id") `);
    }

}
