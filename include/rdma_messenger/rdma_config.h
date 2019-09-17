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

#ifndef RDMA_CONFIG_H
#define RDMA_CONFIG_H

#define SUPPORT_HUGE_PAGE 0
#define HUGE_PAGE_SIZE_2MB (2 * 1024 * 1024)
#define ALIGN_TO_PAGE_2MB(x) \
    (((x) + (HUGE_PAGE_SIZE_2MB - 1)) & ~(HUGE_PAGE_SIZE_2MB - 1))

#define RECV_WQE_PER_QP 64U
#define SEND_WQE_PER_QP 64U

#define SGE_MSG_SIZE (32 * 1024 * 1024U)
#define CQE_PER_CQ 4096

#define IO_WORKER_NUMS 20

#define SUPPORT_SRQ 1
#define SRQ_WQE ((RECV_WQE_PER_QP) * 64)

#define FIN_WRID 0XCAFEBEEF
#define BEACON_WRID 0XDEADBEEF

#endif
