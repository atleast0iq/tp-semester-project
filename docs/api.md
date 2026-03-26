# Battleship TCP API

## Архитектура

Проект разделён на три слоя:

1. `src/common`
   Здесь лежит общий line-delimited JSON протокол, сериализация сообщений и описание флота.
2. `src/server`
   `ServerApplication` принимает TCP-соединения и роутит команды.
   `GameManager` управляет комнатами, очередностью ходов и правилами партии.
   `GameBoard` валидирует расстановку кораблей и обрабатывает выстрелы.
   `DatabaseManager` реализован как singleton над SQLite и хранит пользователей и статистику.
3. `src/client`
   `ApiClient` реализован как singleton и поддерживает одно TCP-соединение к серверу.
   `CommandLineClient` даёт REPL и одноразовые команды для тестирования API без GUI.

## Транспорт

* TCP, UTF-8
* одно JSON-сообщение на строку
* авторизация привязана к TCP-соединению

Формат запроса:

```json
{
  "requestId": "cli-1",
  "action": "login",
  "payload": {
    "username": "alice",
    "password": "secret"
  }
}
```

Формат успешного ответа:

```json
{
  "requestId": "cli-1",
  "status": "ok",
  "payload": {}
}
```

Формат ошибки:

```json
{
  "requestId": "cli-1",
  "status": "error",
  "error": {
    "code": "validation_error",
    "message": "..."
  }
}
```

## Поддерживаемые действия

### `ping`

Проверка доступности сервера.

Payload: пустой объект.

### `register`

Регистрация пользователя и автоматическая авторизация на текущем соединении.

Payload:

```json
{
  "username": "alice",
  "password": "secret"
}
```

### `login`

Авторизация существующего пользователя на текущем соединении.

Payload:

```json
{
  "username": "alice",
  "password": "secret"
}
```

### `logout`

Сбрасывает авторизацию на текущем соединении.

### `whoami`

Возвращает текущего пользователя, его статистику и `currentGameId`, если игра выбрана.

### `stats`

Показывает статистику.

Payload:

```json
{
  "username": "alice"
}
```

Если `username` не передан, сервер вернёт статистику текущего авторизованного пользователя.

### `list_games`

Возвращает список игровых комнат.

### `create_game`

Создаёт новую комнату для текущего пользователя.

### `join_game`

Подключает второго игрока к комнате.

Payload:

```json
{
  "gameId": "5874808b-1e4f-4df4-bf46-9d96a176de36"
}
```

### `place_ships`

Передаёт расстановку флота для текущего игрока.

Payload:

```json
{
  "gameId": "5874808b-1e4f-4df4-bf46-9d96a176de36",
  "ships": [
    { "x": 0, "y": 0, "length": 4, "orientation": "vertical" },
    { "x": 2, "y": 0, "length": 3, "orientation": "vertical" }
  ]
}
```

Полный флот должен соответствовать классической схеме:
`4, 3, 3, 2, 2, 2, 1, 1, 1, 1`

Корабли не должны пересекаться и касаться друг друга.

### `place_random_ships`

Серверная вспомогательная команда для случайной расстановки.

Payload:

```json
{
  "gameId": "5874808b-1e4f-4df4-bf46-9d96a176de36"
}
```

### `game_state`

Возвращает состояние игры для текущего игрока.

Payload:

```json
{
  "gameId": "5874808b-1e4f-4df4-bf46-9d96a176de36"
}
```

Если `gameId` не передан, сервер пробует использовать текущую выбранную игру.

Символы доски:

* `.` — неизвестная или пустая клетка
* `S` — собственный корабль
* `o` — промах
* `X` — попадание

### `fire`

Выстрел по координатам противника.

Payload:

```json
{
  "gameId": "5874808b-1e4f-4df4-bf46-9d96a176de36",
  "x": 4,
  "y": 7
}
```

При промахе ход переходит сопернику. При потоплении всех кораблей сервер обновляет статистику обоих игроков в SQLite.

## Тестовый CLI

Клиент поддерживает:

* `register <username> <password>`
* `login <username> <password>`
* `logout`
* `whoami`
* `stats [username]`
* `list-games`
* `create-game`
* `join-game <gameId>`
* `place-random [gameId]`
* `state [gameId]`
* `fire <x> <y> [gameId]`
* `ping`

Пример интерактивной сессии:

```bash
./build/clang-client-debug/battleship_client --host 127.0.0.1 --port 6767
```

```text
register alice secret
create-game
place-random
state
```
