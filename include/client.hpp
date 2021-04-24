#ifndef __CLINET_HPP__
#define __CLINET_HPP__
#include <nlohmann/json.hpp>
#include <fstream>
using json = nlohmann::json;

class conf;
typedef std::shared_ptr<conf> conf_s;

class client_conf;
class job;

class server_conf;

class job {
public:
  enum job_type { download, upload } type;
  static job create_download_job(const json &js) {
    job cj = job(js);
    cj.type = download;
    return cj;
  }
  static job create_upload_job(const json &js) {
    job cj = job(js);
    cj.type = upload;
    return cj;
  }
  std::string local_file;
  std::string remote_file;

private:
  job(const json &job_node) {
    local_file = job_node["local_file"];
    remote_file = job_node["remote_file"];
  }
};

class client_conf {
public:
  client_conf(const json &js) {
    for (auto &[key, job_json] : js["download"].items()) {
      this->download_job_list.push_back(job::create_download_job(job_json));
    }
    for (auto &[key, job_json] : js["upload"].items()) {
      this->upload_job_list.push_back(job::create_upload_job(job_json));
    }
    this->endpoint = js["endpoint"];
  }

  std::vector<job> download_job_list;
  std::vector<job> upload_job_list;
  std::string endpoint;
};

class conf {
public:
  conf(std::string conf_file) : conf_file(conf_file) {
    std::ifstream conf_istrm(this->conf_file, std::ios::in);
    json conf_json;
    conf_istrm >> conf_json;
    for (auto &[key, value] : conf_json.items()) {
      if (key == std::string("client")) {
        for (auto &[client_key, client_data] : value.items()) {
          client_list.push_back(client_conf(client_data));
        }
      } else if (key == std::string("server")) {
      }
    }
  }
  const std::string conf_file;
  std::vector<client_conf> client_list;
  // std::vector<server_conf> server_list;
};

#endif
