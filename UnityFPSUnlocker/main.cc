#include "main.hh"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>

#include <thread>

#include <absl/container/flat_hash_set.h>
#include <absl/status/statusor.h>

#include "config.hh"
#include "fpslimiter.hh"
#include <logger.hh>
#include <socket.hh>

using namespace rapidjson;

static bool is_loaded = false;
static int frame_rate = 60;
static int delay = 30;
static bool modify_opcode = false;
static absl::flat_hash_set<std::string> target_list;

// In zygiskd memory.
void CompanionEntry(int s) {
    std::string package_name = read_string(s);
    if (is_loaded == false) {
        logger("[UnityFPSUnlocker]initializing...");
        auto read_path = Utility::LoadJsonFromFile("/data/local/tmp/TargetList.json");
        if (!read_path.ok()) {
            logger("LoadJsonFromFile failed: %.*s", (int)read_path.status().message().size(), read_path.status().message().data());
            write_int(s, 0);
            return;
        }
        Document& doc = *read_path;
        if (auto itor = doc.FindMember("packages"); itor != doc.MemberEnd() && itor->value.IsArray()) {
            for (const auto& package : itor->value.GetArray()) {
                target_list.emplace(package.GetString());
            }
        }
        if (auto itor = doc.FindMember("framerate"); itor != doc.MemberEnd() && itor->value.IsInt()) {
            frame_rate = itor->value.GetInt();
        }
        if (auto itor = doc.FindMember("delay"); itor != doc.MemberEnd() && itor->value.IsInt()) {
            delay = itor->value.GetInt();
        }
        if (auto itor = doc.FindMember("modify_opcode"); itor != doc.MemberEnd() && itor->value.IsBool()) {
            modify_opcode = itor->value.GetBool();
        }
        is_loaded = true;
    }

    if (target_list.contains(package_name)) {
        write_int(s, 1);             // is_target : true
        write_int(s, delay);         // delay : delay_
        write_int(s, frame_rate);    // framerate : frame_rate_
        write_int(s, modify_opcode); // framerate : frame_rate_
    } else {
        write_int(s, 0); // is_target : false
    }
}

REGISTER_ZYGISK_MODULE(MyModule)
REGISTER_ZYGISK_COMPANION(CompanionEntry)

void MyModule::onLoad(Api* api, JNIEnv* env) {
    this->api = api;
    this->env = env;
}

void MyModule::preAppSpecialize(AppSpecializeArgs* args) {
    const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
    preSpecialize(process);
    env->ReleaseStringUTFChars(args->nice_name, process);
}

void MyModule::postAppSpecialize(const AppSpecializeArgs* args) {
    if (is_target_) {
        std::thread([=]() {
            FPSLimiter::Start(delay_, framerate_, modify_opcode_);
        }).detach();
    }
}

void MyModule::preSpecialize(const char* process) {
    int client_socket = api->connectCompanion();
    write_string(client_socket, process);
    is_target_ = read_int(client_socket); // is_target

    if (is_target_ == 1) {
        delay_ = read_int(client_socket);
        framerate_ = read_int(client_socket);
        modify_opcode_ = read_int(client_socket);
    }

    close(client_socket);
}