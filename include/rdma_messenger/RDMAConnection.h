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

#ifndef RDMACONNECTION_H
#define RDMACONNECTION_H

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <sys/mman.h>

#include <rdma/rdma_cma.h>

#include <vector>
#include <mutex>

#include "rdma_messenger/rdma_config.h"
#include "rdma_messenger/Callback.h"
#include "rdma_messenger/Buffer.h"
#include "rdma_messenger/Chunk.h"

enum connection_state {
	INACTIVE = 1,
	ACTIVE,
	CLOSE,
};

class RDMAConnection {
	public:
	RDMAConnection(struct ibv_pd* pd, struct ibv_cq* cq, struct rdma_cm_id* cm_id, uint64_t con_id);

	~RDMAConnection();

	void async_send(const char* raw_msg, uint32_t raw_msg_size);
	void async_recv(const char* raw_msg, uint32_t raw_msg_size);

	void async_send_iov(std::vector<const char*> &raw_msg_iov, std::vector<uint32_t> &raw_msg_size);

	// post & recv rdma buffer
	void post_recv_buffer(Chunk* chk);
	void post_recv_buffers();

	uint32_t write_buffer(const char* raw_msg, uint32_t raw_msg_size);
	uint32_t read_buffer(char* raw_msg, uint32_t raw_msg_size);

	// maintain chunk list for posting buffer
	void get_chunk(Chunk **chk);
	void reap_chunk(Chunk **chk);

	struct ibv_qp* get_qp () const;
	struct ibv_cq* get_cq () const;
	uint64_t get_con_id () const;
	bool is_recv_buffer(const char* buf) const;
	bool is_send_buffer(const char* buf) const;

	void set_read_callback(Callback* read_callback);

	void finish();
	void close();

	public:
	connection_state state = INACTIVE;
	Callback *read_callback;

	private:
	// create QP
	void create_qp();

	// register memory region
	void register_mr();
	void construct_chunks(Chunk** chunks, char* buf, uint32_t buf_len, ibv_mr* mr);

	// setup srq
	void create_srq();

	void post_send(const char* raw_msg, uint32_t raw_msg_size);
	void post_send_iov(std::vector<const char*> &raw_msg_iov, std::vector<uint32_t> &raw_msg_size);

	private:
	struct ibv_pd* pd;
	struct ibv_cq* cq;
	struct rdma_cm_id* cm_id;
	uint64_t con_id;
	struct ibv_qp* qp = nullptr;
	struct ibv_srq* srq = nullptr;

	uint32_t recv_buf_len;
	uint32_t send_buf_len;
	char* recv_buf = nullptr;
	char* send_buf = nullptr;
	struct ibv_mr* send_mr = nullptr;
	struct ibv_mr* recv_mr = nullptr;
	Chunk** recv_chunk = nullptr;
	Chunk** send_chunk = nullptr;

	std::vector<Chunk*> free_chunks;
	std::mutex chk_mtx;

	Buffer con_buf;
};

inline void* malloc_huge_pages(size_t size);
inline void free_huge_pages(void *ptr);

#endif
