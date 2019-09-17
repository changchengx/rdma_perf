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

#include <mutex>
#include <chrono>

#include "rdma_messenger/RDMAClient.h"

std::mutex mtx;
uint64_t timestamp_now()
{
	return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
}

class ReadCallback : public Callback {
	public:
	virtual void callback_entry(void* param, void* msg = nullptr) override
	{
		RDMAConnection *con = static_cast<RDMAConnection*>(param);
		Chunk* ck = static_cast<Chunk*>(msg);
		char buf[4096];
		memcpy(buf, ck->chk_buf, ck->chk_size);
		std::unique_lock<std::mutex> lk(mtx);
		if (sum == 0) {
			std::cout << "chunk buf is " << ck->chk_buf << ", chunk size is " << ck->chk_size << std::endl;
			start == timestamp_now();
		}
		sum++;
		if (sum >= 1000000) {
			con->finish();
			end = timestamp_now();
			std::cout << "finished, consume time: " << (end - start)/1000.0 << " seconds" << std::endl;
			return;
		}
		lk.unlock();
		con->async_send("hello server", sizeof("hello server"));
	}
	private:
	uint64_t sum = 0;
	uint64_t start = 0, end = 0;
};

class ConnectCallback : public Callback {
	public:
	virtual ~ConnectCallback()
	{}

	virtual void callback_entry(void* param, void* msg = nullptr) override
	{
		RDMAConnection *con = static_cast<RDMAConnection*>(param);
		assert(read_callback);
		con->set_read_callback(read_callback);
		con->async_send("hello server", sizeof("hello server"));
	}

	void set_read_callback(ReadCallback* read_callback)
	{
		this->read_callback = read_callback;
	}
	private:
	ReadCallback* read_callback = nullptr;
};

int main(int argc, char** argv)
{
	struct addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfo* res = nullptr;
	int n = getaddrinfo("127.0.0.1", "20082", &hints, &res);

	if (n < 0) {
		std::cerr << "failed to get addr info" << std::endl;
	}

	RDMAClient* client = nullptr;
	ConnectCallback* connect_callback = nullptr;
	ReadCallback* read_callback = nullptr;
	for (struct addrinfo* temp = res; temp; temp = temp->ai_next) {
		client = new RDMAClient(temp->ai_addr, 1);
		connect_callback = new ConnectCallback();
		read_callback = new ReadCallback();
		connect_callback->set_read_callback(read_callback);
		client->connect(connect_callback);
	}
	client->wait();

	delete read_callback;
	delete connect_callback;
	delete client;
	return 0;
}
