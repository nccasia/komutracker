import { MigrationInterface, QueryRunner } from "typeorm";

export class PublicHostnameIndex1790934098046 implements MigrationInterface {

    public async up(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`CREATE INDEX IF NOT EXISTS "idx_users_hostname" ON "users" (lower(split_part("email", '@', 1)))`);
    }

    public async down(queryRunner: QueryRunner): Promise<void> {
        await queryRunner.query(`DROP INDEX IF EXISTS "idx_users_hostname"`);
    }

}
