/*
 * types.h
 *
 *  Created on: Apr 25, 2021
 *      Author: ubuntu
 */

#ifndef INCLUDE_PSSC_PROTOCOL_TYPES_H_
#define INCLUDE_PSSC_PROTOCOL_TYPES_H_

#include <cstddef>
#include <cstdint>
#include "pssc/util/Locker.h"

#define BUILD_DEPENDS_ON_PLATFORM

#ifdef BUILD_DEPENDS_ON_PLATFORM

#define SIZE_OF_PSSC_INS sizeof(std::uint8_t)
#define SIZE_OF_PSSC_ID sizeof(std::uint64_t)
#define SIZE_OF_SIZE sizeof(size_t)
#define SIZE_OF_BOOL sizeof(bool)

#else /* not BUILD_DEPENDS_ON_PLATFORM */

#define SIZE_OF_PSSC_INS 1u
#define SIZE_OF_PSSC_ID 8u
#define SIZE_OF_SIZE sizeof(size_t)
#define SIZE_OF_BOOL 1u

#endif /* BUILD_DEPENDS_ON_PLATFORM */

namespace pssc {

using pssc_size = size_t;
using pssc_ins = std::uint8_t;
using pssc_id = std::uint64_t;
using pssc_byte = std::uint8_t;
using pssc_bytes = std::uint8_t*;

using pssc_lock_guard = std::lock_guard<std::mutex>;
using pssc_read_guard = util::unique_read_guard<util::read_write_lock>;
using pssc_write_guard = util::unique_write_guard<util::read_write_lock>;

}

#endif /* INCLUDE_PSSC_PROTOCOL_TYPES_H_ */
