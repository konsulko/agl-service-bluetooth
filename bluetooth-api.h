/*  Copyright 2016 ALPS ELECTRIC CO., LTD.
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*/


#ifndef BLUETOOTH_API_H
#define BLUETOOTH_API_H




//#define _DEBUG
#ifdef _DEBUG
#define D_PRINTF(fmt, args...) \
	printf("[DEBUG][%s:%d:%s]"fmt, __FILE__, __LINE__, __FUNCTION__, ## args)
#define D_PRINTF_RAW(fmt, args...) \
	printf(""fmt, ## args)
#else	/* ifdef _DEBUG */
#define D_PRINTF(fmt, args...)
#define D_PRINTF_RAW(fmt, args...)
#endif	/* ifdef _DEBUG */
#define E_PRINTF(fmt, args...) \
	printf("[ERROR][%s:%d:%s]"fmt, __FILE__, __LINE__, __FUNCTION__, ## args)


/* -------------- PLUGIN DEFINITIONS ----------------- */

typedef struct {
  void *bt_server;          /* handle to implementation  */
  unsigned int index;          /* currently selected media file       */
} BtCtxHandleT;

#endif /* BLUETOOTH_API_H */



/************************************** The End Of File **************************************/  


