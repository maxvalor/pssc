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


#endif /* PSSC_INSTRUCTION_H_ */
