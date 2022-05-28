#include <boost/ut.hpp>
#include <pqrs/osx/json_file_monitor.hpp>

int main(void) {
  using namespace boost::ut;
  using namespace boost::ut::literals;

  "json_file_monitor"_test = [] {
    using namespace std::string_literals;

    auto time_source = std::make_shared<pqrs::dispatcher::hardware_time_source>();
    auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(time_source);

    {
      std::shared_ptr<nlohmann::json> last_json;
      std::string last_json_error_file_path;
      std::string last_json_error_message;

      std::cout << "." << std::flush;

      system("mkdir -p tmp");
      system("echo '{}' > tmp/example.json");

      std::vector<std::string> files{"tmp/example.json"};

      auto json_file_monitor = std::make_shared<pqrs::osx::json_file_monitor>(dispatcher,
                                                                              files);

      json_file_monitor->json_file_changed.connect([&](auto&& changed_file_path, auto&& json) {
        last_json = json;
      });

      json_file_monitor->json_error_occurred.connect([&](auto&& file_path, auto&& message) {
        last_json_error_file_path = file_path;
        last_json_error_message = message;
      });

      json_file_monitor->async_start();

      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      expect(last_json.get() != nullptr);
      expect(*last_json == nlohmann::json::object());
      expect(last_json_error_file_path.empty());
      expect(last_json_error_message.empty());

      system("echo '[1,2,3]' > tmp/example.json");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      expect(last_json.get() != nullptr);
      expect(*last_json == nlohmann::json::array({1, 2, 3}));
      expect(last_json_error_file_path.empty());
      expect(last_json_error_message.empty());

      system("echo '[1' > tmp/example.json");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      expect(last_json.get() != nullptr);
      expect(*last_json == nlohmann::json::array({1, 2, 3}));
      expect(last_json_error_file_path == "tmp/example.json");
      expect(last_json_error_message == "[json.exception.parse_error.101] parse error at line 2, column 1: syntax error while parsing array - unexpected end of input; expected ']'");

      system("rm tmp/example.json");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      expect(last_json.get() == nullptr);
    }

    dispatcher->terminate();
    dispatcher = nullptr;

    std::cout << std::endl;
  };

  return 0;
}
