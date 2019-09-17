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

#include <iostream>
#include "rdma_messenger/RDMAConnection.h"

RDMAConnection::RDMAConnection(struct ibv_pd *pd, struct ibv_cq *cq, struct rdma_cm_id *cm_id, uint64_t con_id) : pd(pd), cq(cq), cm_id(cm_id), con_id(con_id), recv_buf_len(RECV_WQE_PER_QP * SGE_MSG_SIZE), send_buf_len(SEND_WQE_PER_QP * SGE_MSG_SIZE)
{
	recv_buf = static_cast<char*>(memalign(4096, recv_buf_len));
	send_buf = static_cast<char*>(memalign(4096, send_buf_len));
	register_mr();

	recv_chunk = static_cast<Chunk**>(std::calloc(RECV_WQE_PER_QP, sizeof(Chunk*)));
	send_chunk = static_cast<Chunk**>(std::calloc(SEND_WQE_PER_QP, sizeof(Chunk*)));
    construct_chunks(recv_chunk, recv_buf, recv_buf_len, recv_mr);
    construct_chunks(send_chunk, send_buf, send_buf_len, send_mr);
	//* free_chunks??

	if (SUPPORT_SRQ)
		create_srq();
	create_qp();
	state = ACTIVE;
}

RDMAConnection::~RDMAConnection()
{
	for (uint32_t ck_id = 0; ck_id < RECV_WQE_PER_QP; ++ck_id) {
		delete *(recv_chunk + ck_id);
	}
	free(recv_chunk);

	for (uint32_t ck_id = 0; ck_id < SEND_WQE_PER_QP; ++ck_id) {
		delete *(send_chunk + ck_id);
	}
	free(send_chunk);

	//what's about send_buf/recv_buf & registered memory region
}

void RDMAConnection::async_send(const char *raw_msg, uint32_t raw_msg_size)
{
	post_send(raw_msg, raw_msg_size);
}

void RDMAConnection::async_send_iov(std::vector<const char*> &raw_msg, std::vector<uint32_t> &raw_msg_size)
{
	post_send_iov(raw_msg, raw_msg_size);
}

struct ibv_qp* RDMAConnection::get_qp() const
{
	return qp;
}

struct ibv_cq* RDMAConnection::get_cq() const
{
	return cq;
}

uint64_t RDMAConnection::get_con_id() const
{
	return con_id;
}

bool RDMAConnection::is_recv_buffer(const char *buf) const
{
	if (buf >= recv_buf && buf <= recv_buf+recv_buf_len-1)
		return true;
	return false;
}

bool RDMAConnection::is_send_buffer(const char *buf) const
{
	if (buf >= send_buf && buf <= send_buf + send_buf_len - 1)
		return true;
	return false;
}

uint32_t RDMAConnection::write_buffer(const char *raw_msg, uint32_t raw_msg_size)
{
	if (con_buf.write_buf(raw_msg, raw_msg_size) > 0) {
		return raw_msg_size;
	}
	return 0;
}

uint32_t RDMAConnection::read_buffer(char *raw_msg, uint32_t raw_msg_size)
{
	if (con_buf.read_buf(raw_msg, raw_msg_size) > 0)
		return raw_msg_size;
	return 0;
}

void RDMAConnection::set_read_callback(Callback *read_callback) {
	this->read_callback = read_callback;
}

void RDMAConnection::close()
{
	printf("close connection.\n");
	std::cout << "close connection." << std::endl;
	delete this;
}

void RDMAConnection::post_recv_buffers()
{
	int32_t ret = 0;

	struct ibv_recv_wr* bad_wr = nullptr;
	struct ibv_sge recv_sge = {};
	struct ibv_recv_wr recv_wr = {};
	for (uint32_t ck_id = 0; ck_id < RECV_WQE_PER_QP; ++ck_id) {
		Chunk* chk = recv_chunk[ck_id];
		recv_sge.addr = (uintptr_t) chk->chk_buf;
		recv_sge.length = SGE_MSG_SIZE;
		recv_sge.lkey = chk->mr->lkey;

		recv_wr.sg_list = &recv_sge;
		recv_wr.num_sge = 1;
		recv_wr.next = NULL;
		recv_wr.wr_id = (uint64_t)chk;
		if (SUPPORT_SRQ) {
			ret = ibv_post_srq_recv(srq, &recv_wr, &bad_wr);
		} else {
			ret = ibv_post_recv(qp, &recv_wr, &bad_wr);
		}
		if (ret) {
			std::cerr << __func__ << " failed to post recv wrs " << std::endl;
		}
	}
}

void RDMAConnection::post_recv_buffer(Chunk *ck) {

	struct ibv_recv_wr* bad_wr = nullptr;
	struct ibv_sge recv_sge = {};
	struct ibv_recv_wr recv_wr = {};

	recv_sge.addr = (uintptr_t) (ck->chk_buf);
	recv_sge.length = SGE_MSG_SIZE;
	recv_sge.lkey = ck->mr->lkey;

	recv_wr.sg_list = &recv_sge;
	recv_wr.num_sge = 1;
	recv_wr.next = nullptr;
	recv_wr.wr_id = reinterpret_cast<uint64_t>(ck);

	int32_t ret = 0;
	if (SUPPORT_SRQ) {
		ret = ibv_post_srq_recv(srq, &recv_wr, &bad_wr);
	} else {
		ret = ibv_post_recv(qp, &recv_wr, &bad_wr);
	}
	if (ret) {
		std::cerr << __func__ << " failed to post recv wr " << std::endl;
	}
}

void RDMAConnection::post_send(const char *raw_msg, uint32_t raw_msg_size) {
	struct ibv_send_wr *bad_wr = nullptr;
	struct ibv_sge send_sge = {};
	struct ibv_send_wr send_wr = {};
	Chunk *ck = nullptr;
	get_chunk(&ck);
	assert(ck);
	assert(raw_msg_size <= SGE_MSG_SIZE);
	memcpy(ck->chk_buf, (char*)raw_msg, raw_msg_size);
	ck->chk_size = raw_msg_size;

	send_sge.addr = (uintptr_t) ck->chk_buf;
	send_sge.length = raw_msg_size;
	send_sge.lkey = ck->mr->lkey;

	send_wr.opcode = IBV_WR_SEND;
	send_wr.send_flags = IBV_SEND_SIGNALED;
	send_wr.sg_list = &send_sge;
	send_wr.num_sge = 1;
	send_wr.wr_id = reinterpret_cast<uint64_t>(ck);
	send_wr.next = NULL;
	int ret = ibv_post_send(qp, &send_wr, &bad_wr);
	if (ret) {
		std::cerr << __func__ << " failed to post send wrs " << std::endl;
	}
}

void RDMAConnection::post_send_iov(std::vector<const char*> &raw_msg_iov, std::vector<uint32_t> &raw_msg_size)
{
	uint32_t sge_index = 0;
	int chunk_size = raw_msg_iov.size();
	struct ibv_sge send_sge[chunk_size] = {};
	struct ibv_send_wr send_wr[chunk_size] = {};
	ibv_send_wr* pre_wr = nullptr;
	for (uint32_t base = 0; base < chunk_size; ++base) {
		assert(raw_msg_size[base] <= SGE_MSG_SIZE);

		Chunk *ck = nullptr;
		get_chunk(&ck);
		assert(ck);

		memcpy(ck->chk_buf, raw_msg_iov[base], raw_msg_size[base]);
		ck->chk_size = raw_msg_size[base];

		send_sge[sge_index].addr = (uintptr_t) ck->chk_buf;
		send_sge[sge_index].length = raw_msg_size[base];
		send_sge[sge_index].lkey = ck->mr->lkey;

		send_wr[sge_index].opcode = IBV_WR_SEND;
		send_wr[sge_index].send_flags = IBV_SEND_SIGNALED;
		send_wr[sge_index].sg_list = &send_sge[sge_index];
		send_wr[sge_index].num_sge = 1;
		send_wr[sge_index].wr_id = reinterpret_cast<uint64_t>(ck);
		send_wr[sge_index].next = NULL;
		if (pre_wr)
			pre_wr->next = &send_wr[sge_index];
		pre_wr = &send_wr[sge_index];
		sge_index++;
	}

	struct ibv_send_wr *bad_wr = nullptr;
	int ret = ibv_post_send(qp, send_wr, &bad_wr);
	if (ret) {
		std::cerr << __func__ << " failed to post send wrs " << std::endl;
	}
}

void RDMAConnection::finish()
{
	struct ibv_send_wr send_wr = {};
	struct ibv_sge send_sge = {};
	struct ibv_send_wr *bad_wr = nullptr;
	memset(&send_wr, 0, sizeof(send_wr));

	Chunk *ck = NULL;
	get_chunk(&ck);
	assert(ck);

	send_sge.addr = (uintptr_t) ck->chk_buf;
	send_sge.length = 0;
	send_sge.lkey = ck->mr->lkey;

	send_wr.wr_id = FIN_WRID;
	send_wr.sg_list = &send_sge;
	send_wr.num_sge = 1;
	send_wr.opcode = IBV_WR_SEND;
	send_wr.send_flags = IBV_SEND_SIGNALED;
	send_wr.next = NULL;
	int ret = ibv_post_send(qp, &send_wr, &bad_wr);
	if (ret) {
		std::cerr<< __func__ << " ibv send error " << std::endl;
	}
}

void RDMAConnection::create_qp() {
	struct ibv_qp_init_attr init_attr;
	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.cap.max_send_wr = SEND_WQE_PER_QP;
	init_attr.cap.max_recv_wr = RECV_WQE_PER_QP;
	init_attr.cap.max_recv_sge = 1;
	init_attr.cap.max_send_sge = 1;
	init_attr.qp_type = IBV_QPT_RC;
	init_attr.send_cq = cq;
	init_attr.recv_cq = cq;
	if (SUPPORT_SRQ)
		init_attr.srq = srq;

	rdma_create_qp(cm_id, pd, &init_attr);
	qp = cm_id->qp;
}

void RDMAConnection::create_srq()
{
	ibv_srq_init_attr sia = {};
	sia.attr.max_wr = SRQ_WQE;
	sia.attr.max_sge = 1;
	srq = ibv_create_srq(pd, &sia);
}

void RDMAConnection::register_mr()
{
	recv_mr = ibv_reg_mr(pd, recv_buf, recv_buf_len, IBV_ACCESS_LOCAL_WRITE | \
			                                         IBV_ACCESS_REMOTE_READ | \
			                                         IBV_ACCESS_REMOTE_WRITE);

    send_mr = ibv_reg_mr(pd, send_buf, send_buf_len, IBV_ACCESS_LOCAL_WRITE | \
			                                         IBV_ACCESS_REMOTE_READ | \
			                                         IBV_ACCESS_REMOTE_WRITE);
}

void RDMAConnection::construct_chunks(Chunk** chunks, char* buf, uint32_t buf_len, ibv_mr* mr)
{
	uint32_t ck_id = 0;
	for (uint32_t offset = 0; offset < buf_len; offset += SGE_MSG_SIZE) {
		Chunk* ck = new Chunk(mr, buf + offset, SGE_MSG_SIZE);
		*(chunks + ck_id) = ck;
		ck_id++;
	}
}

void RDMAConnection::get_chunk(Chunk **ck) {
	std::lock_guard<std::mutex> l(chk_mtx);
	if (free_chunks.size() == 0)
		return;
	*ck = free_chunks.back();
	assert(*ck);
	free_chunks.pop_back();
}

void RDMAConnection::reap_chunk(Chunk **ck)
{
	std::lock_guard<std::mutex> l(chk_mtx);
	assert(*ck);
	free_chunks.push_back(*ck);
}


void *malloc_huge_pages(size_t size)
{
	size_t real_size = ALIGN_TO_PAGE_2MB(size + HUGE_PAGE_SIZE_2MB);
	char *ptr = (char *)mmap(NULL, real_size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB, -1, 0);
	if (ptr == MAP_FAILED) {
		ptr = static_cast<char *>(std::malloc(real_size));
		if (ptr == nullptr)
			return nullptr;
		real_size = 0;
	}
	*((size_t *)ptr) = real_size;
	return ptr + HUGE_PAGE_SIZE_2MB;
}

void free_huge_pages(void *ptr)
{
	if (ptr == nullptr)
		return;
	void *real_ptr = (char *)ptr - HUGE_PAGE_SIZE_2MB;
	size_t real_size = *((size_t *)real_ptr);
	assert(real_size % HUGE_PAGE_SIZE_2MB == 0);
	if (real_size != 0) {
		munmap(real_ptr, real_size);
	} else {
		free(real_ptr);
	}
}
