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

#ifndef BUFFER_H
#define BUFFER_H

#include <memory>

class Buffer {
	public:
	Buffer(uint32_t buf_len = 4 * 1024 * 1024) : max_len(buf_len) {
		buf = new char [max_len * sizeof(char)];
	}

	~Buffer() {
		delete buf;
		buf = nullptr;
	}

	uint32_t write_buf(const char *raw_msg, uint32_t raw_msg_size) {
		uint32_t wrote_len = 0;
		uint32_t wait_len = raw_msg_size;
		const char* copy_start = raw_msg;
		while(true) {
			wrote_len = new_round ? readable - writeable : max_len - writeable;
			wrote_len = wrote_len >= wait_len ? wait_len : wrote_len;
			memcpy(buf + writeable, copy_start, wrote_len);

			writeable += wrote_len;
			wait_len -= wrote_len;
			copy_start += wrote_len;

			if (!new_round && writeable == max_len) {
				writeable = 0;
				new_round = true;
			}

			if (writeable == readable || wait_len == 0)
				return raw_msg_size - wait_len;
		}
	}

    uint32_t read_buf(char *raw_msg, uint32_t raw_msg_size) {
		uint32_t read_len = 0;
		uint32_t wait_len = raw_msg_size;
		char* copy_start = raw_msg;

		while(true) {
			read_len = new_round ? max_len - readable : writeable - readable;
			read_len = read_len >= wait_len ? wait_len : read_len;
			memcpy(copy_start, buf + readable, read_len);

			readable += read_len;
			wait_len -= read_len;
			copy_start += read_len;

			if (new_round && readable == max_len) {
				readable = 0;
				new_round = false;
			}

			if (writeable == readable || wait_len == 0)
				return raw_msg_size - wait_len;
		}
    }
  private:
    bool new_round = false;
    uint32_t readable = 0;
    uint32_t writeable = 0;
    uint32_t max_len;
    char *buf = nullptr;
};

#endif
