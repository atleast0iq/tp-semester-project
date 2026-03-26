# Морской бой / Battleship

![Battleship](./assets/ship.jpg)

## О проекте
Учебный проект в рамках курса "Технологии и методы программирования". На текущем этапе реализует сетевой MVP игры "Морской бой" с клиент-серверной архитектурой на `Qt + TCP`, SQLite-хранилищем и консольным клиентом для тестирования API.

## Команда
* Варламов Егор Михайлович
* Иляков Иван Константинович
* Хайруллин Тахир Зиннурович

## Текущий статус
Сейчас в репозитории реализован MVP клиент-серверного приложения на `Qt + TCP`:
* `battleship_server` — сервер с JSON API, игровой логикой "Морского боя" и SQLite
* `battleship_client` — консольный клиент для тестирования API без GUI
* `src/common` — общий протокол и утилиты флота для сервера и клиента



##️ GUI клиент

Реализован клиент с графическим интерфейсом (на Qt Widgets).

### Возможности:

- Подключение к серверу
- Регистрация + автроизация пользователя
- Просмотр активных игр
- Отображение текущего пользователя  
- Обработка сетевых ошибок  


Поддерживаются:
* регистрация и авторизация пользователей
* хранение статистики в SQLite
* создание комнаты, подключение второго игрока
* случайная или пользовательская расстановка флота через API
* ходы, проверка попаданий и обновление статистики

Описание архитектуры и API:
* `docs/api.md`

## Сборка
Проект использует `CMake` и компилятор `clang++`.

Собрать оба приложения:
```bash
cmake --preset clang-debug
cmake --build --preset build-debug
```

Собрать только клиент:
```bash
cmake --preset clang-client-debug
cmake --build --preset build-client
```

Собрать только сервер:
```bash
cmake --preset clang-server-debug
cmake --build --preset build-server
```

Собрать оба приложения в релизной версии:
```bash
cmake --preset clang-release
cmake --build --preset build-release
```

## Запуск
Запустить сервер:
```bash
./build/{preset}/battleship_server --address 0.0.0.0 --port 6767 --database battleship.db
```

Запустить консольный клиент:
```bash
./build/{preset}/battleship_client --host 127.0.0.1 --port 6767
```

Пример команд в клиенте:
```text
register alice secret
create-game
place-random
state
```

## Docker

Собрать и запустить сервер через Docker Compose:
```bash
docker compose up --build
```
SQLite используется сервером как встраиваемая файловая БД в Docker volume `battleship_data`.
