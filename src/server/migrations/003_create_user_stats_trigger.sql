CREATE TRIGGER IF NOT EXISTS users_after_insert_create_stats
AFTER INSERT ON users
BEGIN
    INSERT INTO user_stats(user_id, games_played, wins, losses, shots, hits)
    VALUES (NEW.id, 0, 0, 0, 0, 0);
END;
