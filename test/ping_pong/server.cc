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

#include <chrono>

#include "rdma_messenger/RDMAServer.h"

class ReadCallback : public Callback {
	public:
	virtual void callback_entry(void *param, void *msg = nullptr) override {
		RDMAConnection *con = static_cast<RDMAConnection*>(param);
		Chunk *ck = static_cast<Chunk*>(msg);
		char buf[4096];
		memcpy(buf, ck->chk_buf, ck->chk_size);
		con->async_send("hello client", sizeof("hello client"));
    }
};

class AcceptCallback : public Callback {
	public:
	virtual void callback_entry(void *param, void *msg = nullptr) override {
		RDMAConnection *con = static_cast<RDMAConnection*>(param);
		assert(read_callback);
		con->set_read_callback(read_callback);
	}
	void set_read_callback(ReadCallback *read_callback) {
		this->read_callback = read_callback;
	}
	private:
	ReadCallback *read_callback = nullptr;
};

int main() {
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(20082);
	sin.sin_addr.s_addr = INADDR_ANY;

	RDMAServer *server = new RDMAServer((struct sockaddr*)&sin);
	AcceptCallback *accept_callback = new AcceptCallback();
	ReadCallback *read_callback = new ReadCallback();
	accept_callback->set_read_callback(read_callback);

	server->start(accept_callback);
	server->wait();

	delete read_callback;
	delete accept_callback;
	delete server;
	return 0;
}
