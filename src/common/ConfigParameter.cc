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

#include "common/ConfigParameter.h"

ConfigParameter::ConfigParameter(const int parameter_count, const char** parameter_vec) :
	qp_config(this, QPCONFIG), rq_config(this, RQCONFIG), sq_config(this, SQCONFIG),
	wqe_config(this, WQECONFIG), sge_config(this, SGECONFIG)
{
	std::vector<const char*> parameters;
	parameters.insert(parameters.end(), parameter_vec, parameter_vec + parameter_count);

	for (auto& a : parameters) {
		orig_parameters.push_back(a);
	}

	for (auto& a : parameters) {
		if (strcmp("-c", a) == 0 || strcmp("--conf", a) == 0) {
			yaml_config_file = std::next(a);
			break;
		}
	}
	add_registers();
}

ConfigParameter::ConfigParameter() :
	qp_config(this, QPCONFIG), rq_config(this, RQCONFIG), sq_config(this, SQCONFIG),
	wqe_config(this, WQECONFIG), sge_config(this, SGECONFIG)
{
	add_registers();
}

ConfigParameter* ConfigParameter::GetConfigObj() {
	return configobj;
}

ConfigParameter* ConfigParameter::CreateConfigObj(const int parameter_count, const char** parameter_vec)
{
	if (configobj == nullptr) {
		configobj = new ConfigParameter(parameter_count, parameter_vec);
	}
	return configobj;
}

ConfigParameter::ConfigSubscriber::ConfigSubscriber(ConfigParameter* center_parameter, CONFIG_TYPE config_type) :
	center_parameter(center_parameter), self_config_type(config_type)
{}

void ConfigParameter::ConfigPublish::add_register(ConfigSubscriber* subscriber) {
	subscribers.push_back(subscriber);
}

void ConfigParameter::ConfigPublish::detach_publish(const ConfigSubscriber* subscriber) {
	auto iter = std::find(subscribers.begin(), subscribers.end(), subscriber);
	if (iter != subscribers.end()) {
		subscribers.erase(iter);
	}
}

ConfigParameter::UPDATE_OPTION ConfigParameter::ConfigPublish::broadcast_config(CONFIG_TYPE config_type) const {
	for (auto& subscriber: subscribers) {
		auto rst = subscriber->update_config(config_type);
		if (rst != CONFIG_UPDATE_DONE) {
			return rst;
		}
	}
	return CONFIG_UPDATE_DONE;
}

ConfigParameter::QPConfig::QPConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type) :
	ConfigSubscriber(center_parameter, config_type),
	qp_config(center_parameter->configs.qp_config)
{}

ConfigParameter::UPDATE_OPTION ConfigParameter::QPConfig::update_config(CONFIG_TYPE config_type) {
	if (self_config_type == config_type) {
	}
	return CONFIG_UPDATE_DONE;
}

ConfigParameter::RQConfig::RQConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type) :
	ConfigSubscriber(center_parameter, config_type),
	rq_config(center_parameter->configs.rq_config)
{}

ConfigParameter::UPDATE_OPTION ConfigParameter::RQConfig::update_config(CONFIG_TYPE config_type) {
	if (self_config_type == config_type) {
	}
	return CONFIG_UPDATE_DONE;
}

ConfigParameter::SQConfig::SQConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type) :
	ConfigSubscriber(center_parameter, config_type),
	sq_config(center_parameter->configs.sq_config)	
{}

ConfigParameter::UPDATE_OPTION ConfigParameter::SQConfig::update_config(CONFIG_TYPE config_type) {
	if (self_config_type == config_type) {
	}
	return CONFIG_UPDATE_DONE;
}

ConfigParameter::WQEConfig::WQEConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type) :
	ConfigSubscriber(center_parameter, config_type),
	wqe_config(center_parameter->configs.wqe_config)
{}

ConfigParameter::UPDATE_OPTION ConfigParameter::WQEConfig::update_config(CONFIG_TYPE config_type) {
	if (self_config_type == config_type) {
	}
	return CONFIG_UPDATE_DONE;
}

ConfigParameter::SGEConfig::SGEConfig(ConfigParameter* center_parameter, CONFIG_TYPE config_type) :
	ConfigSubscriber(center_parameter, config_type),
	sge_config(center_parameter->configs.sge_config)
{}

ConfigParameter::UPDATE_OPTION ConfigParameter::SGEConfig::update_config(CONFIG_TYPE config_type) {
	if (self_config_type == config_type) {
	}
	return CONFIG_UPDATE_DONE;
}

void ConfigParameter::ConfigParse()
{
	YAML::Node yaml_config = YAML::LoadFile(yaml_config_file.c_str());

	const YAML::Node& yaml_qp_config = yaml_config["qp"];
	ParseQP(yaml_qp_config);

	const YAML::Node& yaml_rq_config = yaml_config["rq"];
	ParseRQ(yaml_rq_config);

	const YAML::Node& yaml_sq_config = yaml_config["sq"];
	ParseSQ(yaml_sq_config);

	const YAML::Node& yaml_wqe_config = yaml_config["wqe"];
	ParseWQE(yaml_wqe_config);

	const YAML::Node& yaml_sge_config = yaml_config["sge"];
	ParseSGE(yaml_sge_config);

	const YAML::Node& yaml_hugepage_config = yaml_config["hugepage"];
	ParsePage(yaml_hugepage_config);

	const YAML::Node& yaml_cm_config = yaml_config["connection"];
	ParseCM(yaml_cm_config);

	const YAML::Node& yaml_server_config = yaml_config["server"];
	ParseServer(yaml_server_config);

	const YAML::Node& yaml_client_config = yaml_config["client"];
	ParseClient(yaml_client_config);

	const YAML::Node& yaml_test_config = yaml_config["test"];
	ParseTest(yaml_test_config);
}

void ConfigParameter::add_registers() {
	config_publish.add_register(&qp_config);
	config_publish.add_register(&rq_config);
	config_publish.add_register(&sq_config);
	config_publish.add_register(&wqe_config);
	config_publish.add_register(&sge_config);
}

void ConfigParameter::ParseQP(const YAML::Node& yaml_qp_config) {
	std::string qp_transport_mode = yaml_qp_config["qp_transport_mode"].as<std::string>();
	configs.qp_config.qp_transport_mode = strcmp(qp_transport_mode.c_str(), "RC") == 0 ? IBV_QPT_RC :
										static_cast<ibv_qp_type>(static_cast<int>(IBV_QPT_DRIVER) + 1);
	configs.qp_config.qp_create_method = strcmp(yaml_qp_config["qp_create_method"].as<std::string>().c_str(), "verbs") == 0 ? VERBS_QP :
										 strcmp(yaml_qp_config["qp_create_method"].as<std::string>().c_str(), "rdma_cm") ? RDMACM_QP : UNKNOWN_CREATE_QP_METHOD;

	configs.qp_config.qp_timeout = yaml_qp_config["qp_timeout"].as<uint8_t>();
	configs.qp_config.retry_cnt = yaml_qp_config["retry_cnt"].as<uint8_t>();
	configs.qp_config.rnr_retry = yaml_qp_config["rnr_retry"].as<uint8_t>();
}

void ConfigParameter::ParseRQ(const YAML::Node& yaml_rq_config) {
	configs.rq_config.srq = strcmp(yaml_rq_config["srq"].as<std::string>().c_str(), "true") == 0 ? true : false;
	configs.rq_config.srq_depths_sq = yaml_rq_config["srq_depths_sq"].as<uint16_t>();
	configs.rq_config.rq_depth = yaml_rq_config["rq_depth"].as<uint32_t>();
}

void ConfigParameter::ParseSQ(const YAML::Node& yaml_sq_config) {
	configs.sq_config.sq_depth = yaml_sq_config["sq_depth"].as<uint32_t>();
	configs.sq_config.burst_wr_max = yaml_sq_config["burst_wr_max"].as<uint32_t>();
}

void ConfigParameter::ParseWQE(const YAML::Node& yaml_wqe_config) {
	configs.wqe_config.use_inline = strcmp(yaml_wqe_config["use_inline"].as<std::string>().c_str(), "true") == 0 ? true : false;
	configs.wqe_config.inline_size = yaml_wqe_config["inline_size"].as<uint32_t>();
	configs.wqe_config.sge_per_wqe = yaml_wqe_config["sge_per_wqe"].as<uint32_t>();
}

void ConfigParameter::ParseSGE(const YAML::Node& yaml_sge_config) {
	configs.sge_config.sge_length = yaml_sge_config["sge_length"].as<uint32_t>();
}

void ConfigParameter::ParsePage(const YAML::Node& yaml_hugepage_config) {
	configs.use_huge_page = strcmp(yaml_hugepage_config["use_hugepage"].as<std::string>().c_str(), "true") == 0 ? true : false;
}

void ConfigParameter::ParseCM(const YAML::Node& yaml_cm_config) {
	std::string cm_connection_method = yaml_cm_config["connection_method"].as<std::string>();
	configs.cm_config.cm_establish = strcmp(cm_connection_method.c_str(), "rdma_cm") == 0 ? CM_RDMA_ESTABLISH :
									 strcmp(cm_connection_method.c_str(), "tcp_cm") ? CM_TCP_ESTABLISH : CM_UNKNOWN_ESTABLISH;
	strncpy(configs.cm_config.server_ip_addr, yaml_cm_config["server_ip_addr"].as<std::string>().c_str(), sizeof(configs.cm_config.server_ip_addr));
	configs.cm_config.server_cm_port = yaml_cm_config["server_cm_port"].as<uint32_t>();
}

void ConfigParameter::ParseServer(const YAML::Node& yaml_server_config) {
	strncpy(configs.server_config.server_ip_addr, yaml_server_config["server_ip_addr"].as<std::string>().c_str(), sizeof(configs.server_config.server_ip_addr));
	configs.server_config.server_cm_port = yaml_server_config["server_cm_port"].as<uint32_t>();
	configs.server_config.server_threads = yaml_server_config["server_threads"].as<uint32_t>();
}

void ConfigParameter::ParseClient(const YAML::Node& yaml_client_config) {
	strncpy(configs.client_config.client_ip_addr, yaml_client_config["client_ip_addr"].as<std::string>().c_str(), sizeof(configs.client_config.client_ip_addr));
	configs.client_config.client_threads = yaml_client_config["client_threads"].as<uint32_t>();
	configs.client_config.data_per_thread = yaml_client_config["data_per_thread"].as<uint64_t>();
}

void ConfigParameter::ParseTest(const YAML::Node& yaml_test_config) {
	configs.test_config.time_duration = yaml_test_config["time_duration"].as<uint64_t>();
}
