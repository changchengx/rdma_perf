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

#ifndef RNICAFFINITY_H
#define RNICAFFINITY_H

#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <numa.h>

#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include <map>

class RNICAffinity {
	public:
	RNICAffinity(const char* addr);

    void query_rnic_eth_name();

	void query_rnic_ib_name();

	void query_rnic_ib_port();

	private:
	void scan_ib(std::vector<std::string>& ib_devs);
	void query_rnic_numa_node();
	void query_rnic_cpumask();

	private:
	struct sockaddr_storage rnic = {.ss_family = AF_INET,};
	std::string rnic_eth_name;
	std::string rnic_ib_name;
	uint16_t rnic_ib_port = 0;
	std::string rnic_addr;
	std::string ib_path = "/sys/class/infiniband/";
	std::string eth_path = "/sys/class/net/";
	uint32_t bind_numa = -1U;
	unsigned long long cpu_mask[2 * sizeof(unsigned long long)] = {};
};
#endif
