# Drop-in style calls against local WebSearchFree server (Tavily-shaped).
#
#   cmake --build build
#   ./build/wsf serve --port 8080
#
# Then:
#   curl -s http://127.0.0.1:8080/health
#   curl -s -X POST http://127.0.0.1:8080/search ^
#     -H "Content-Type: application/json" ^
#     -d "{\"query\":\"open source metasearch\",\"max_results\":5}"
#   curl -s -X POST http://127.0.0.1:8080/extract ^
#     -H "Content-Type: application/json" ^
#     -d "{\"url\":\"https://example.com\"}"
