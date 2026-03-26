CREATE VIEW IF NOT EXISTS user_statistics_view AS
SELECT
    u.id AS user_id,
    u.username AS username,
    s.games_played AS games_played,
    s.wins AS wins,
    s.losses AS losses,
    s.shots AS shots,
    s.hits AS hits
FROM users AS u
JOIN user_stats AS s ON s.user_id = u.id;
