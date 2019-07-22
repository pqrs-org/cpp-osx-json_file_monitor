#include <csignal>
#include <iostream>
#include <pqrs/osx/json_file_monitor.hpp>

int main(void) {
  auto time_source = std::make_shared<pqrs::dispatcher::hardware_time_source>();
  auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(time_source);

  system("mkdir -p tmp");
  system("echo '{}' > tmp/example.json");

  std::vector<std::string> files{"tmp/example.json"};

  auto json_file_monitor = std::make_shared<pqrs::osx::json_file_monitor>(dispatcher,
                                                                          files);

  json_file_monitor->json_file_changed.connect([](auto&& changed_file_path, auto&& json) {
    if (json) {
      std::cout << "json: " << *json << std::endl;
    } else {
      std::cout << "json: null" << std::endl;
    }
  });

  json_file_monitor->json_error_occurred.connect([](auto&& file_path, auto&& message) {
    std::cerr << "json_error_occurred: " << file_path << " " << message << std::endl;
  });

  json_file_monitor->async_start();

  // ============================================================

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  system("echo '{\"hello\":\"world\"}' > tmp/example.json");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  system("echo '{\"hello\":\"world\"' > tmp/example.json");

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // ============================================================

  json_file_monitor = nullptr;

  dispatcher->terminate();
  dispatcher = nullptr;

  return 0;
}
