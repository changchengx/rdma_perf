/*
 * Copyright 2019 Liu Changcheng <changcheng.liu@aliyun.com>
 * Author: Liu Changcheng <changcheng.liu@aliyun.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CONFIGPARAMETER_H
#define CONFIGPARAMETER_H

#include <infiniband/verbs.h>
#include <yaml-cpp/yaml.h>

enum RUN_ENVIRONMENT {
	SERVER_ENVIRONMENT = 1,
	CLIENT_ENVIRONMENT,
	UNKNOWN_ENVIRONMENT
};

enum CREATE_QP {
	VERBS_QP = 1,
	RDMACM_QP,
	UNKNOWN_CREATE_QP_METHOD,
};

enum CM_ESTABLISH {
	CM_RDMA_ESTABLISH = 1,
	CM_TCP_ESTABLISH,
	CM_UNKNOWN_ESTABLISH,
};

struct qp_config_value {
	ibv_qp_type qp_transport_mode = static_cast<ibv_qp_type>(static_cast<int>(IBV_QPT_DRIVER) + 1);
	CREATE_QP qp_create_method = VERBS_QP;
	uint8_t qp_timeout = 18;
	uint8_t retry_cnt = 7;
	uint8_t rnr_retry = 7;
};

struct rq_config_value {
	bool srq = true;
	uint16_t srq_depths_sq = 10;
	uint32_t rq_depth = 4096;
};

struct sq_config_value {
	uint32_t sq_depth = 4096;
	uint32_t burst_wr_max = 1024;
};

struct wqe_config_value {
	bool use_inline = false;
	uint32_t inline_size = 128;
	uint32_t sge_per_wqe = 1;
};

struct sge_config_value {
	uint32_t sge_length = 4096;
};

struct cm_establish_value {
	CM_ESTABLISH cm_establish = CM_RDMA_ESTABLISH;
	char server_ip_addr[128] = {0};
	uint32_t server_cm_port = 160603;
};

struct server_config_value {
	char server_ip_addr[128] = {0};
	uint32_t server_cm_port = 160603;
	uint32_t server_threads = 1;
};

struct client_config_value {
	char client_ip_addr[128] = {0};
	uint32_t client_threads = 10;
	uint64_t data_per_thread = 0x800000000;
};

struct test_config_value {
	uint64_t time_duration = 10000;
};

class ConfigParameter {
	private:
	ConfigParameter(const int parameter_count, const char** parameter_vec);

	ConfigParameter();

	public:
	static ConfigParameter* GetConfigObj();

	static ConfigParameter* CreateConfigObj(const int parameter_count, const char** parameter_vec);

	void ConfigParse();

	private:
	static ConfigParameter* configobj;

	public:
	enum CONFIGVALUE_VER {
		VERSION_19930616 = 19930616,
	};

	struct ConfigValue {
		CONFIGVALUE_VER value_version = VERSION_19930616;
		struct qp_config_value qp_config;
		struct rq_config_value rq_config;
		struct sq_config_value sq_config;
		struct wqe_config_value wqe_config;
		struct sge_config_value sge_config;
		struct cm_establish_value cm_config;
		struct server_config_value server_config;
		struct client_config_value client_config;
		bool use_huge_page = false;
		struct test_config_value test_config;
	} configs;

	enum CONFIG_TYPE {
		QPCONFIG = 1,
		RQCONFIG,
		SQCONFIG,
		WQECONFIG,
		SGECONFIG
	};

	enum UPDATE_OPTION {
		CONFIG_REUPDATE,
		CONFIG_UPDATE_DONE,
		CONFIG_STOP,
	};

	class ConfigSubscriber {
		public:
		virtual UPDATE_OPTION update_config(CONFIG_TYPE) = 0;

		ConfigSubscriber(ConfigParameter* center_parameter, CONFIG_TYPE config_type);

		protected:
		ConfigParameter* center_parameter;
		CONFIG_TYPE self_config_type;
	};

	class ConfigPublish {
		public:
		std::vector<ConfigSubscriber*> subscribers;

		void add_register(ConfigSubscriber* subscriber);

		void detach_publish(const ConfigSubscriber* subscriber);

		UPDATE_OPTION broadcast_config(CONFIG_TYPE config_type) const;
	};

	class QPConfig: public ConfigSubscriber {
		public:
		QPConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type);

		UPDATE_OPTION update_config(CONFIG_TYPE config_type);

		public:
		struct qp_config_value& qp_config;
	};

	class RQConfig: public ConfigSubscriber {
		public:
		RQConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type);

		UPDATE_OPTION update_config(CONFIG_TYPE config_type);

		public:
		struct rq_config_value rq_config;
	};

	class SQConfig: public ConfigSubscriber {
		public:
		SQConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type);

		UPDATE_OPTION update_config(CONFIG_TYPE config_type);

		public:
		struct sq_config_value& sq_config;
	};

	class WQEConfig: public ConfigSubscriber {
		public:
		WQEConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type);

		UPDATE_OPTION update_config(CONFIG_TYPE config_type);

		public:
		struct wqe_config_value& wqe_config;
	};

	class SGEConfig: public ConfigSubscriber {
		public:
		SGEConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type);

		UPDATE_OPTION update_config(CONFIG_TYPE config_type);

		public:
		struct sge_config_value& sge_config;
	};

	private:
	std::vector<std::string> orig_parameters;
	std::string yaml_config_file = "config.yaml";
	ConfigPublish config_publish;

	QPConfig qp_config;
	RQConfig rq_config;
	SQConfig sq_config;
	WQEConfig wqe_config;
	SGEConfig sge_config;

	private:
	void add_registers();

	void ParseQP(const YAML::Node& yaml_qp_config);

	void ParseRQ(const YAML::Node& yaml_rq_config);

	void ParseSQ(const YAML::Node& yaml_sq_config);

	void ParseWQE(const YAML::Node& yaml_wqe_config);

	void ParseSGE(const YAML::Node& yaml_sge_config);

	void ParsePage(const YAML::Node& yaml_hugepage_config);

	void ParseCM(const YAML::Node& yaml_cm_config);

	void ParseServer(const YAML::Node& yaml_server_config);

	void ParseClient(const YAML::Node& yaml_client_config);

	void ParseTest(const YAML::Node& yaml_test_config);

};

ConfigParameter* ConfigParameter::configobj = nullptr;

#endif
