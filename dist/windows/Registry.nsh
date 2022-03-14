!define registry::Open `!insertmacro registry::Open`

!macro registry::Open _PATH _OPTIONS _HANDLE
	registry::_Open /NOUNLOAD `${_PATH}` `${_OPTIONS}`
	Pop ${_HANDLE}
!macroend


!define registry::Find `!insertmacro registry::Find`

!macro registry::Find _HANDLE _PATH _VALUEORKEY _STRING _TYPE
	registry::_Find /NOUNLOAD `${_HANDLE}`
	Pop ${_PATH}
	Pop ${_VALUEORKEY}
	Pop ${_STRING}
	Pop ${_TYPE}
!macroend


!define registry::Close `!insertmacro registry::Close`

!macro registry::Close _HANDLE
	registry::_Close /NOUNLOAD `${_HANDLE}`
!macroend


!define registry::KeyExists `!insertmacro registry::KeyExists`

!macro registry::KeyExists _PATH _ERR
	registry::_KeyExists /NOUNLOAD `${_PATH}`
	Pop ${_ERR}
!macroend


!define registry::Read `!insertmacro registry::Read`

!macro registry::Read _PATH _VALUE _STRING _TYPE
	registry::_Read /NOUNLOAD `${_PATH}` `${_VALUE}`
	Pop ${_STRING}
	Pop ${_TYPE}
!macroend


!define registry::Write `!insertmacro registry::Write`

!macro registry::Write _PATH _VALUE _STRING _TYPE _ERR
	registry::_Write /NOUNLOAD `${_PATH}` `${_VALUE}` `${_STRING}` `${_TYPE}`
	Pop ${_ERR}
!macroend


!define registry::ReadExtra `!insertmacro registry::ReadExtra`

!macro registry::ReadExtra _PATH _VALUE _NUMBER _STRING _TYPE
	registry::_ReadExtra /NOUNLOAD `${_PATH}` `${_VALUE}` `${_NUMBER}`
	Pop ${_STRING}
	Pop ${_TYPE}
!macroend


!define registry::WriteExtra `!insertmacro registry::WriteExtra`

!macro registry::WriteExtra _PATH _VALUE _STRING _ERR
	registry::_WriteExtra /NOUNLOAD `${_PATH}` `${_VALUE}` `${_STRING}`
	Pop ${_ERR}
!macroend


!define registry::CreateKey `!insertmacro registry::CreateKey`

!macro registry::CreateKey _PATH _ERR
	registry::_CreateKey /NOUNLOAD `${_PATH}`
	Pop ${_ERR}
!macroend


!define registry::DeleteValue `!insertmacro registry::DeleteValue`

!macro registry::DeleteValue _PATH _VALUE _ERR
	registry::_DeleteValue /NOUNLOAD `${_PATH}` `${_VALUE}`
	Pop ${_ERR}
!macroend


!define registry::DeleteKey `!insertmacro registry::DeleteKey`

!macro registry::DeleteKey _PATH _ERR
	registry::_DeleteKey /NOUNLOAD `${_PATH}`
	Pop ${_ERR}
!macroend


!define registry::DeleteKeyEmpty `!insertmacro registry::DeleteKeyEmpty`

!macro registry::DeleteKeyEmpty _PATH _ERR
	registry::_DeleteKeyEmpty /NOUNLOAD `${_PATH}`
	Pop ${_ERR}
!macroend


!define registry::CopyValue `!insertmacro registry::CopyValue`

!macro registry::CopyValue _PATH_SOURCE _VALUE_SOURCE _PATH_TARGET _VALUE_TARGET _ERR
	registry::_CopyValue /NOUNLOAD `${_PATH_SOURCE}` `${_VALUE_SOURCE}` `${_PATH_TARGET}` `${_VALUE_TARGET}`
	Pop ${_ERR}
!macroend


!define registry::MoveValue `!insertmacro registry::MoveValue`

!macro registry::MoveValue _PATH_SOURCE _VALUE_SOURCE _PATH_TARGET _VALUE_TARGET _ERR
	registry::_MoveValue /NOUNLOAD `${_PATH_SOURCE}` `${_VALUE_SOURCE}` `${_PATH_TARGET}` `${_VALUE_TARGET}`
	Pop ${_ERR}
!macroend


!define registry::CopyKey `!insertmacro registry::CopyKey`

!macro registry::CopyKey _PATH_SOURCE _PATH_TARGET _ERR
	registry::_CopyKey /NOUNLOAD `${_PATH_SOURCE}` `${_PATH_TARGET}`
	Pop ${_ERR}
!macroend


!define registry::MoveKey `!insertmacro registry::MoveKey`

!macro registry::MoveKey _PATH_SOURCE _PATH_TARGET _ERR
	registry::_MoveKey /NOUNLOAD `${_PATH_SOURCE}` `${_PATH_TARGET}`
	Pop ${_ERR}
!macroend


!define registry::SaveKey `!insertmacro registry::SaveKey`

!macro registry::SaveKey _PATH _FILE _OPTIONS _ERR
	registry::_SaveKey /NOUNLOAD `${_PATH}` `${_FILE}` `${_OPTIONS}`
	Pop ${_ERR}
!macroend


!define registry::RestoreKey `!insertmacro registry::RestoreKey`

!macro registry::RestoreKey _FILE _ERR
	registry::_RestoreKey /NOUNLOAD `${_FILE}`
	Pop ${_ERR}
!macroend


!define registry::StrToHex `!insertmacro registry::StrToHexA`
!define registry::StrToHexA `!insertmacro registry::StrToHexA`

!macro registry::StrToHexA _STRING _HEX_STRING
	registry::_StrToHexA /NOUNLOAD `${_STRING}`
	Pop ${_HEX_STRING}
!macroend


!define registry::StrToHexW `!insertmacro registry::StrToHexW`

!macro registry::StrToHexW _STRING _HEX_STRING
	registry::_StrToHexW /NOUNLOAD `${_STRING}`
	Pop ${_HEX_STRING}
!macroend


!define registry::HexToStr `!insertmacro registry::HexToStrA`
!define registry::HexToStrA `!insertmacro registry::HexToStrA`

!macro registry::HexToStrA _HEX_STRING _STRING
	registry::_HexToStrA /NOUNLOAD `${_HEX_STRING}`
	Pop ${_STRING}
!macroend

!define registry::HexToStrW `!insertmacro registry::HexToStrW`

!macro registry::HexToStrW _HEX_STRING _STRING
	registry::_HexToStrW /NOUNLOAD `${_HEX_STRING}`
	Pop ${_STRING}
!macroend


!define registry::Unload `!insertmacro registry::Unload`

!macro registry::Unload
	registry::_Unload
!macroend
