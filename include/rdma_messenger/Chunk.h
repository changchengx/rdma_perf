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

#ifndef CHUNK_H
#define CHUNK_H

#include <rdma/rdma_cma.h>

class Chunk {
	public:
	Chunk(struct ibv_mr *mr, char* chk_buf, uint32_t chk_size):
		mr(mr), chk_buf(chk_buf), chk_size(chk_size)
	{}

	struct ibv_mr *mr;
	char* chk_buf;
	uint32_t chk_size;
};

#endif
