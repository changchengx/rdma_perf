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

#include "rdma_messenger/RNICAffinity.h"

RNICAffinity::RNICAffinity(const char* addr): rnic_addr(addr)
{
	struct addrinfo* res = nullptr;
	int rst = getaddrinfo(addr, nullptr, nullptr, &res);
	if (rst) {
		std::cerr << "getaddrinfo failed: " << gai_strerror(rst) << " invalid IP address " << std::endl;
		return;
	}
	std::unique_ptr<addrinfo, void(*)(addrinfo*)> finally{res, freeaddrinfo};
	struct sockaddr* rnic_socket = reinterpret_cast<struct sockaddr *>(&rnic);
	switch (res->ai_family) {
	case PF_INET:
		memcpy(rnic_socket, res->ai_addr, sizeof(struct sockaddr_in));
		break;
	case PF_INET6:
		memcpy(rnic_socket, res->ai_addr, sizeof(struct sockaddr_in6));
		break;
	default:
		std::cerr << "Invalid sockaddr family: " << res->ai_family << '\n';
		return;
	}

	query_rnic_eth_name();
	query_rnic_ib_name();
	query_rnic_ib_port();

	if (numa_available() != -1) {
		query_rnic_numa_node();
		query_rnic_cpumask();
	}
	return;
}

void RNICAffinity::query_rnic_eth_name()
{
	ifaddrs* ifa = nullptr;
	if (getifaddrs(&ifa) == -1) {
		std::cerr << "getifaddrs: " << strerror(errno) << std::endl;
		return;
	}
	std::unique_ptr<ifaddrs, void(*)(ifaddrs*)> finally{ifa, freeifaddrs};

	auto ss = rnic.ss_family;
	const sockaddr_in *nic_in = ss == AF_INET ? reinterpret_cast<const sockaddr_in*>(&rnic) : nullptr;
	const sockaddr_in6 *nic_in6 = ss == AF_INET6 ? reinterpret_cast<const sockaddr_in6*>(&rnic) : nullptr;

	for (; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr || !ifa->ifa_name || ifa->ifa_addr->sa_family != ss) {
			continue;
		}
		const sockaddr_in *ifa_in = ss == AF_INET ? reinterpret_cast<const sockaddr_in*>(ifa->ifa_addr) : nullptr;
		const sockaddr_in6 *ifa_in6 = ss == AF_INET6 ? reinterpret_cast<const sockaddr_in6*>(ifa->ifa_addr) : nullptr;

		bool rst = ss == AF_INET ? ifa_in->sin_addr.s_addr == nic_in->sin_addr.s_addr :
						memcmp(ifa_in6->sin6_addr.s6_addr, nic_in6->sin6_addr.s6_addr, sizeof(nic_in6->sin6_addr.s6_addr)) == 0;
		if (rst == true) {
			rnic_eth_name = ifa->ifa_name;
			return;
		}
	}
}

void RNICAffinity::query_rnic_numa_node()
{
	if (numa_available() == -1) {
		return;
	}
	std::ifstream numa_file(eth_path + rnic_eth_name + "/device/numa_node");
	if(numa_file) {
		numa_file >> bind_numa;
		numa_file.close();
	}
}

void RNICAffinity::query_rnic_cpumask()
{
	if (numa_available() == -1 || bind_numa == -1) {
		return;
	}
	struct bitmask* mask = numa_allocate_cpumask();
	int rst = numa_node_to_cpus(bind_numa, mask);
	if (rst < 0)
		goto clean;
	for (size_t i = 0; i < mask->size; i++) {
		if (numa_bitmask_isbitset(mask, i)) {
			cpu_mask[i / (8 * sizeof(unsigned long long))] =
				cpu_mask[i / (8 * sizeof(unsigned long long))] | \
				(1 << (i % (8 * sizeof(unsigned long long))));
		}
	}
clean:
	numa_free_cpumask(mask);
}

void RNICAffinity::scan_ib(std::vector<std::string>& ib_devs)
{
	DIR* dirp = opendir(ib_path.c_str());
	struct dirent* dp = nullptr;
	while ((dp = readdir(dirp)) != nullptr) {
		if (strcmp(".", dp->d_name) == 0 || strcmp("..", dp->d_name) == 0)
			continue;
		ib_devs.push_back(dp->d_name);
	}
	closedir(dirp);
}

void RNICAffinity::query_rnic_ib_name()
{
	std::vector<std::string> ib_devs;
	scan_ib(ib_devs);

	std::map<size_t, std::string> ibs_resource;
	std::ifstream resource_file;
	for (auto &dev_name : ib_devs) {
		resource_file.open((ib_path + dev_name + "/device/resource").c_str(), std::ios::in|std::ios::binary|std::ios::ate);
		if (resource_file) {
			unsigned resource_len = resource_file.tellg();
			char* resource_mem = new char [resource_len + 1]{0};
			resource_file.seekg(0, std::ios::beg);
			resource_file.read(resource_mem, resource_len);
			resource_file.close();
			size_t hash_val = std::hash<std::string>()(std::string(resource_mem));
			ibs_resource.insert(std::pair<size_t, std::string>(hash_val, dev_name));
		}
	}

	size_t eth_resource_key = 0;
	resource_file.open((eth_path + rnic_eth_name + "/device/resource").c_str(), std::ios::in|std::ios::binary|std::ios::ate);
	if (resource_file) {
		unsigned resource_len = resource_file.tellg();
		char* resource_mem = new char [resource_len + 1]{0};
		resource_file.seekg(0, std::ios::beg);
		resource_file.read(resource_mem, resource_len);
		resource_file.close();
		eth_resource_key = std::hash<std::string>()(std::string(resource_mem));
	}
	auto it = ibs_resource.find(eth_resource_key);
	if (it != ibs_resource.end()) {
		rnic_ib_name = it->second;
	}
}

void RNICAffinity::query_rnic_ib_port()
{
	std::ifstream ifile;

	uint16_t net_devid = 0;
	ifile.open((eth_path + rnic_eth_name + "/dev_id").c_str());
	if (ifile) {
		ifile >> net_devid;
		ifile.close();
	}
	uint16_t ib_port = net_devid;

	uint16_t net_port = 0;
	ifile.open((eth_path + rnic_eth_name + "/dev_port").c_str());
	if (ifile) {
		ifile >> net_port;
		ifile.close();
	}

	if (net_port > net_devid) {
		ib_port = net_port;
	}

	rnic_ib_port = ib_port + 1;
}
