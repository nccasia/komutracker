import { MigrationInterface, QueryRunner } from "typeorm";

export class IdxEndStartIdEvents1791085543617 implements MigrationInterface {
    name = 'IdxEndStartIdEvents1791085543617'

    public async up(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`CREATE INDEX "idx_afk_events_user_end_start" ON "afk_events" ("user_id", "end_at", "start_at", "id") `);
    }

    public async down(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`DROP INDEX "public"."idx_afk_events_user_end_start"`);
    }

}
