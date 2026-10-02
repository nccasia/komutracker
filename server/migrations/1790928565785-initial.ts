import { MigrationInterface, QueryRunner } from 'typeorm';

export class Initial1790928565785 implements MigrationInterface {
    name = 'Initial1790928565785';

    async up(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`CREATE EXTENSION IF NOT EXISTS "uuid-ossp"`);
        await queryRunner.query(`CREATE TABLE "users" ("id" uuid NOT NULL DEFAULT uuid_generate_v4(), "device_id" character varying(128) NOT NULL, "mezon_id" character varying(255), "name" character varying(255) NOT NULL, "email" character varying(320) NOT NULL, "auth_token" character varying(2048), "created_at" TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT now(), "updated_at" TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT now(), "last_seen_at" TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT now(), CONSTRAINT "UQ_users_device_id" UNIQUE ("device_id"), CONSTRAINT "UQ_users_mezon_id" UNIQUE ("mezon_id"), CONSTRAINT "PK_users" PRIMARY KEY ("id"))`);
        await queryRunner.query(`CREATE TABLE "afk_events" ("id" BIGSERIAL NOT NULL, "user_id" uuid NOT NULL, "status" character varying(16) NOT NULL, "start_at" TIMESTAMP WITH TIME ZONE NOT NULL, "end_at" TIMESTAMP WITH TIME ZONE NOT NULL, "created_at" TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT now(), CONSTRAINT "PK_afk_events" PRIMARY KEY ("id"))`);
        await queryRunner.query(`CREATE INDEX "idx_afk_events_user_start" ON "afk_events" ("user_id", "start_at")`);
    }

    async down(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`DROP INDEX "public"."idx_afk_events_user_start"`);
        await queryRunner.query(`DROP TABLE "afk_events"`);
        await queryRunner.query(`DROP TABLE "users"`);
    }
}
