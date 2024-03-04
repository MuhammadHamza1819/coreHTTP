#ifndef INCLUDE_MOCK_LLHTTP_H_
#define INCLUDE_MOCK_LLHTTP_H_



#ifdef __cplusplus
extern "C" {
#endif

#define llhttp_init_Ignore();
#define llhttp_init_Stub( llhttp_init_setup );
#define llhttp_get_errno_Stub( llhttp_get_errno_cb );
#define llhttp_settings_init_Ignore();
#define llhttp_execute_Stub( llhttp_execute_whole_response );
#define llhttp_execute_ExpectAnyArgsAndReturn( HPE_OK );

#define llhttp_settings_init_AddCallback( parserSettingsInitExpectationCb );
#define llhttp_init_AddCallback( parserInitExpectationCb );
#define llhttp_execute_AddCallback( parserExecuteExpectationsCb );
#define llhttp_settings_init_ExpectAnyArgs();
#define llhttp_init_ExpectAnyArgs();


#endif
