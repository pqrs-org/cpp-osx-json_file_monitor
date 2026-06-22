#include <boost/ut.hpp>
#include <pqrs/osx/json_file_monitor.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <utility>

int main() {
  using namespace boost::ut;
  using namespace boost::ut::literals;

  "json_file_monitor"_test = [] {
    using namespace std::string_literals;

    auto time_source = std::make_shared<pqrs::dispatcher::hardware_time_source>();
    auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(time_source);

    {
      std::shared_ptr<nlohmann::json> last_json;
      size_t json_file_changed_count = 0;
      std::string last_error_message;
      std::string last_json_error_file_path;
      std::string last_json_error_message;
      size_t json_error_occurred_count = 0;
      std::mutex mutex;
      std::condition_variable condition;

      std::cout << "." << std::flush;

      const auto file_path = std::filesystem::absolute(std::filesystem::path("tmp/example.json"));
      const auto file_path_string = file_path.string();
      std::filesystem::remove_all(file_path.parent_path());
      expect(std::system("mkdir -p tmp") == 0);
      expect(std::system("echo '[1,2,3]' > tmp/example.json") == 0);

      std::vector<std::string> files{file_path_string};

      auto json_file_monitor = std::make_shared<pqrs::osx::json_file_monitor>(dispatcher,
                                                                              files);

      expect(json_file_monitor->attached());

      json_file_monitor->json_file_changed.connect([&](auto&& changed_file_path, auto&& json) {
        expect(dispatcher->dispatcher_thread());
        expect(changed_file_path == file_path_string);

        std::lock_guard lock(mutex);
        last_json = json;
        ++json_file_changed_count;
        condition.notify_all();
      });

      json_file_monitor->json_error_occurred.connect([&](auto&& file_path, auto&& message) {
        expect(dispatcher->dispatcher_thread());

        std::lock_guard lock(mutex);
        last_json_error_file_path = file_path;
        last_json_error_message = message;
        ++json_error_occurred_count;
        condition.notify_all();
      });

      json_file_monitor->error_occurred.connect([&](auto&& message) {
        expect(dispatcher->dispatcher_thread());

        std::lock_guard lock(mutex);
        last_error_message = message;
        condition.notify_all();
      });

      json_file_monitor->async_start();

      auto wait_for = [&](auto&& predicate) {
        std::unique_lock lock(mutex);
        return condition.wait_for(lock,
                                  std::chrono::seconds(5),
                                  std::forward<decltype(predicate)>(predicate));
      };

      if (expect(wait_for([&] {
            return last_json != nullptr ||
                   !last_error_message.empty();
          }))) {
        std::lock_guard lock(mutex);
        expect(last_error_message.empty());
        if (last_json) {
          expect(*last_json == nlohmann::json::array({1, 2, 3}));
          expect(last_json_error_file_path.empty());
          expect(last_json_error_message.empty());
        }
      }

      // Use an external process to avoid kFSEventStreamEventFlagOwnEvent.
      expect(std::system("echo '{}' > tmp/example.json") == 0);
      wait_for([&] {
        return json_file_changed_count >= 2;
      });
      const auto json_file_changed_count_before_json_error = json_file_changed_count;

      // Use an external process to avoid kFSEventStreamEventFlagOwnEvent.
      expect(std::system("echo '[1' > tmp/example.json") == 0);

      if (expect(wait_for([&] {
            return json_error_occurred_count >= 1;
          }))) {
        std::lock_guard lock(mutex);
        expect(last_json_error_file_path == file_path_string);
        expect(last_json != nullptr);
        expect(json_file_changed_count == json_file_changed_count_before_json_error);
        expect(last_error_message.empty());
        expect(last_json_error_message == "[json.exception.parse_error.101] parse error at line 2, column 1: syntax error while parsing array - unexpected end of input; expected ']'");
      }

      expect(std::system("rm tmp/example.json") == 0);

      expect(wait_for([&] {
        return last_json == nullptr;
      }));
    }

    dispatcher->terminate();
    dispatcher = nullptr;

    std::cout << std::endl;
  };

  return 0;
}
