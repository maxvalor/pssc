/*
 * commands.h
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#ifndef PSSC_INSTRUCTION_H_
#define PSSC_INSTRUCTION_H_

enum Instruction
{
	REGISTER = 0x00,
	REGACK,
	UNREGISTER,
	ADDVERTISE_TOPIC,
	SUBSCRIBE,
	SUBACK,
	UNSUBSCRIBE,
	PUBLISH,
	ADDVERTISE_SERVICE,
	ADVSRVACK,
	SERVICE_CALL,
	SERVICE_RESPONSE,
	UNKOWN,
};

using Ins = Instruction;

#define INS_SIZE sizeof(std::uint8_t)
#define ID_SIZE sizeof(std::uint64_t)
#define SIZE_SIZE sizeof(size_t)

#endif /* PSSC_INSTRUCTION_H_ */
