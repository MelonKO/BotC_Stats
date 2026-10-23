-- Migration: a player's end side can only be good or evil
--
-- Background: `alignment` is the side of the conflict, and the value set differs
-- per context (see core/docs/SIDES.md). A role may be neutral (for example a
-- Traveller, whose side the storyteller picks), but the player always ends the
-- game on the good or the evil side, so `games_import_staging.alignment_end_ru`
-- must not accept 'нейтральный'.
--
-- Safe to run on a fresh database: 01-schema.sql already creates the narrowed
-- constraint, and this migration only replaces it when it still allows
-- 'нейтральный'. Staged rows with a neutral end side are discarded because
-- staging is transient import data.

DO $$
BEGIN
    IF to_regclass('public.games_import_staging') IS NULL THEN
        RAISE NOTICE 'games_import_staging does not exist, nothing to migrate';
        RETURN;
    END IF;

    DELETE FROM games_import_staging
    WHERE alignment_end_ru NOT IN ('добро', 'зло');

    ALTER TABLE games_import_staging
        DROP CONSTRAINT IF EXISTS games_import_staging_alignment_end_ru_check;

    ALTER TABLE games_import_staging
        ADD CONSTRAINT games_import_staging_alignment_end_ru_check
        CHECK (alignment_end_ru IN ('добро', 'зло'));
END
$$;
