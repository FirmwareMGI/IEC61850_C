import ctypes
try:
    import lib61850_linux as lib61850
except:
    import lib61850
import libiec61850client as iec61850client

import argparse
import json
import logging
import time
from enum import Enum
logger = logging.getLogger('IEC61850')
logging.basicConfig(format='%(asctime)s %(name)-12s %(levelname)-8s %(message)s',
        level=logging.INFO)

import faulthandler
faulthandler.enable()
#import redis
REDIS_HOST = '192.168.2.19'


json_test = {
  "id": 1,
  "ip": "127.0.0.1",
  "port": 102,
  "protocol": "iec61850_client"
}

def main():
    parser = argparse.ArgumentParser(description="Load a JSON file.")
    parser.add_argument('-j', '--json', required=True, help='Path to the JSON file')

    args = parser.parse_args()

    try:
        with open(args.json, 'r') as file:
            data = json.load(file)
            print("Loaded JSON data:", data)
    except Exception as e:
        print(f"Error reading JSON file: {e}")

class AddCause(Enum):
    ADD_CAUSE_UNKNOWN = 0
    ADD_CAUSE_NOT_SUPPORTED = 1
    # ot successful since one of the downstream Loc switches like in CSWI has the value TRUE
    ADD_CAUSE_BLOCKED_BY_SWITCHING_HIERARCHY = 2
    ADD_CAUSE_SELECT_FAILED = 3 					# e.g. wrong ctlnum, or reserved by second client?
    ADD_CAUSE_INVALID_POSITION = 4					# cmdterm -
    ADD_CAUSE_POSITION_REACHED = 5					# if element was there allready
    # Control action is blocked due to running parameter change.
    ADD_CAUSE_PARAMETER_CHANGE_IN_EXECUTION = 6
    ADD_CAUSE_STEP_LIMIT = 7						# tapchanger end pos
    ADD_CAUSE_BLOCKED_BY_MODE = 8					# if mod is set to local
    ADD_CAUSE_BLOCKED_BY_PROCESS = 9				# e.g. blocked by eehealth
    ADD_CAUSE_BLOCKED_BY_INTERLOCKING = 10			# blocked by interlocking
    ADD_CAUSE_BLOCKED_BY_SYNCHROCHECK = 11			# blocked by synchrocheck
    ADD_CAUSE_COMMAND_ALREADY_IN_EXECUTION = 12		# operate is ongoing
    ADD_CAUSE_BLOCKED_BY_HEALTH = 13				# blocked by health
    # blocked by any of the relevant controls(stSeld of another related CSWI)
    ADD_CAUSE_1_OF_N_CONTROL = 14
    ADD_CAUSE_ABORTION_BY_CANCEL = 15				# action is cancelled
    ADD_CAUSE_TIME_LIMIT_OVER = 16					# switch did not reach in time
    ADD_CAUSE_ABORTION_BY_TRIP = 17					# overridden control by trip
    ADD_CAUSE_OBJECT_NOT_SELECTED = 18				# not selected
    ADD_CAUSE_OBJECT_ALREADY_SELECTED = 19			# we had it allready
    ADD_CAUSE_NO_ACCESS_AUTHORITY = 20				# ...
    ADD_CAUSE_ENDED_WITH_OVERSHOOT = 21				# end position overshot
    # diff between command and measured val
    ADD_CAUSE_ABORTION_DUE_TO_DEVIATION = 22
    ADD_CAUSE_ABORTION_BY_COMMUNICATION_LOSS = 23  # con loss
    # Control action is blocked due to the data attribute CmdBlk.stVal is TRUE.
    ADD_CAUSE_ABORTION_BY_COMMAND = 24
    ADD_CAUSE_NONE = 25								# success
    # change in test/ctlnum between select/oper
    ADD_CAUSE_INCONSISTENT_PARAMETERS = 26
    ADD_CAUSE_LOCKED_BY_OTHER_CLIENT = 27			# another client selected it

def printValue(value):
    _type = lib61850.MmsValue_getTypeString(value)
    _type = str(_type)
    if _type == "boolean":
        return ("%r" % lib61850.MmsValue_getBoolean(value)), _type
    if _type == "array":
        return ("arr"), _type
    if _type == "bcd":
        return ("bcd"), _type
    if _type == "binary-time":
        return ("%i" % lib61850.MmsValue_getBinaryTimeAsUtcMs(value)), _type
    if _type == "bit-string":
        return ("%i" % lib61850.MmsValue_getBitStringAsInteger(value)), _type
    if _type == "access-error":
        return ("ACCESS ERROR"), _type
    if _type == "float":
        return ("%f" % lib61850.MmsValue_toFloat(value)), _type
    if _type == "generalized-time":
        return ("%u" % lib61850.MmsValue_toUnixTimestamp(value)), _type
    if _type == "integer":
        return ("%i" % lib61850.MmsValue_toInt64(value)), _type
    if _type == "oid":
        return ("OID ERROR"), _type
    if _type == "mms-string":
        return ("%s" % lib61850.MmsValue_toString(value).decode("utf-8")), _type
    if _type == "structure":
        return value, _type
    if _type == "octet-string":
        len = lib61850.MmsValue_getOctetStringSize(value)
        buf = lib61850.MmsValue_getOctetStringBuffer(value)
        # magic cast to convert a swig pointer into a ctypes pointer, the int(buf) works, but why?
        buff = ctypes.cast(buf, ctypes.POINTER(ctypes.c_char))
        # allocate a buffer for the result
        res = bytearray(len)
        # create a pointer to the result buffer
        rptr = (ctypes.c_char * len).from_buffer(res)
        # copy the memory from the swig buffer to the result buffer
        ctypes.memmove(rptr, buff, len)
        return ("%s" % ''.join(format(x, '02x') for x in res)), _type
    if _type == "unsigned":
        return ("%u" % lib61850.MmsValue_toUint32(value)), _type
    if _type == "utc-time":
        return ("%u" % lib61850.MmsValue_getUtcTimeInMs(value)), _type
    if _type == "visible-string":
        return ("%s" % lib61850.MmsValue_toString(value).decode("utf-8")), _type
    if _type == "unknown(error)":
        return ("UNKNOWN ERROR"), _type
    return ("CANNOT FIND TYPE"), _type

def getMMsValue(typeVal, value, size=8, typeval=-1):
    # allocate mmsvalue based on type
    if typeVal == "visible-string" or typeval == lib61850.MMS_VISIBLE_STRING:
        return lib61850.MmsValue_newVisibleString(str(value))
    if typeVal == "boolean" or typeval == lib61850.MMS_BOOLEAN:
        if (type(value) is str and value.lower() == "true") or (type(value) is bool and value == True):
            return lib61850.MmsValue_newBoolean(True)
        else:
            return lib61850.MmsValue_newBoolean(False)
    if typeVal == "integer" or typeval == lib61850.MMS_INTEGER:
        return lib61850.MmsValue_newInteger(int(value))
    # untested
    if typeVal == "unsigned" or typeval == lib61850.MMS_UNSIGNED:
        return lib61850.MmsValue_newUnsignedFromUint32(int(value))
    if typeVal == "mms-string" or typeval == lib61850.MMS_STRING:
        return lib61850.MmsValue_newMmsString(str(value))
    if typeVal == "float" or typeval == lib61850.MMS_FLOAT:
        return lib61850.MmsValue_newFloat(float(value))
    if typeVal == "binary-time" or typeval == lib61850.MMS_BINARY_TIME:
        return lib61850.MmsValue_newBinaryTime(int(value))
    if typeVal == "bit-string" or typeval == lib61850.MMS_BIT_STRING:
        bs = lib61850.MmsValue_newBitString(size)
        return lib61850.MmsValue_setBitStringFromInteger(bs, int(value))
    if typeVal == "generalized-time" or typeval == lib61850.MMS_GENERALIZED_TIME:
        return lib61850.MmsValue_newUtcTimeByMsTime(int(value))
    if typeVal == "utc-time" or typeval == lib61850.MMS_UTC_TIME:
        return lib61850.MmsValue_newUtcTimeByMsTime(int(value))
    if typeVal == "octet-string" or typeval == lib61850.MMS_OCTET_STRING:
        sl = len(value)
        sptr = (ctypes.c_char * sl).from_buffer(value)

        buf = lib61850.MmsValue_newOctetString(sl, 127)
        buff = ctypes.cast(int(buf), ctypes.POINTER(ctypes.c_char))

        ctypes.memmove(buff, sptr, sl)
        return buf
    # unsupported types
    if typeVal == "array" or typeval == lib61850.MMS_ARRAY:
        return None
    if typeVal == "bcd" or typeval == lib61850.MMS_BCD:
        return None
    if typeVal == "access-error" or typeval == lib61850.MMS_DATA_ACCESS_ERROR:
        return None
    if typeVal == "oid" or typeval == lib61850.MMS_OBJ_ID:
        return None
    if typeVal == "structure" or typeval == lib61850.MMS_STRUCTURE:
        return None
    if typeVal == "unknown(error)":
        return None
    logger.error("Mms value type %s not supported" % typeVal)
    return None

def commandTerminationHandler(param, con):
    # print("ASU")
    buff = ctypes.cast(param, ctypes.POINTER(ctypes.c_char))
    # buff = ctypes.cast(param, ctypes.c_char_p).value.decode("utf-8")
    # logger.debug("commandTerminationHandler_cb called: %s", buff)
    lastApplError = lib61850.ControlObjectClient_getLastApplError(con)

    # /* if lastApplError.error != 0 this indicates a CommandTermination- */
    # if self.cmdTerm_cb != None:

    if lastApplError.error != 0:
        addCause = AddCause(lastApplError.addCause).name
        print("Received CommandTermination-.\n")
        print(" LastApplError: %i\n"% (lastApplError.error))
        print("      addCause: %i\n"% (lastApplError.addCause))
        # self.cmdTerm_cb("object:%s Received CommandTermination-, LastApplError: %i, addCause: %s" %
        #                 (buff, lastApplError.error, addCause))
    else:
        print("Received CommandTermination+.")
        # self.cmdTerm_cb(
        #     "object:%s Received CommandTermination+" % buff)

def cmd(param, con):
    print("ASU")

if __name__ == "__main__":
    # main()

    tcpPort = 102
    host =   "192.168.2.16" #"172.26.80.1" #(WINDOWS FROM WSL)
    
    error = lib61850.IedClientError()
    con = lib61850.IedConnection_create()
    lib61850.IedConnection_connect(con,ctypes.byref(error), host, tcpPort)
    if (error.value == lib61850.IED_ERROR_OK):
        print("IEC61850 CONNECTED")

        ## SELECT WITH ENHANCED SECURITY
        '''
        object_control_sbo_security = "D101Ctrl/CSWI1.Pos"
        object_control_sbo_val = "D101Ctrl/CSWI1.Pos.stVal"
        control = lib61850.ControlObjectClient_create(object_control_sbo_security, con)
        if  control != None:
   
            cbh = lib61850.CommandTerminationHandler(commandTerminationHandler)
            lib61850.ControlObjectClient_setCommandTerminationHandler(control, cbh, None)
            
            # newValue = None
            # value = lib61850.IedConnection_readObject(
            # con, ctypes.byref(error), object_control_sbo_val, lib61850.IEC61850_FC_ST)
            # if error.value == lib61850.IED_ERROR_OK:
            #     read_value, read_type = printValue(value)
            #     print("Value '%s' type %s from item %s" %
            #                 (str(read_value), read_type, object_control_sbo_val))
            #     newValue = read_value
            #     lib61850.MmsValue_delete(value)
            # print(newValue)
                    
            ctlVal = lib61850.MmsValue_newBoolean(True)
            select = lib61850.ControlObjectClient_selectWithValue(control, ctlVal)
            if select:
                print("select: return succesfull")
                error = lib61850.ControlObjectClient_operate(control,ctlVal,0)
                if error == 1:
                    print("operate: returned succesfull")
                    # _val, _type = printValue(ctlVal)
                    # print(_val, _type)
                else:
                    print("operate: %s returned failed")
                    # _val, _type = printValue(ctlVal)
                    # print(_val, _type)
                    lastApplError = lib61850.ControlObjectClient_getLastApplError(
                        control)
                    addCause = AddCause(lastApplError.addCause).name
                    print("LastApplError: %i, addCause: %s" %
                                (lastApplError.error, addCause))
            else:
                print("select: return unsuccesfull")
            time.sleep(1)

            lib61850.MmsValue_delete(ctlVal)
        '''

        ## DIRECT NORMAL ##
        object_control_direct_security = "BCUCPLCONTROL1/GBAY1.Pos"
        object_control_direct_val = "BCUCPLCONTROL1/GBAY1.Pos.stVal"  
        control = lib61850.ControlObjectClient_create("BCUCPLCONTROL1/GBAY1.Pos", con)
        # print(control)
        if  control != None:
   
            cbh = lib61850.CommandTerminationHandler(commandTerminationHandler)
            lib61850.ControlObjectClient_setCommandTerminationHandler(control, cbh, None)
                    
            ctlVal = lib61850.MmsValue_newBoolean(True)
            lib61850.ControlObjectClient_setOrigin(control, None, 3);

            error = lib61850.ControlObjectClient_operate(control,ctlVal,0)
            if error == 1:
                print("operate: returned succesfull")
                # _val, _type = printValue(ctlVal)
                # print(_val, _type)
            else:
                print("operate: %s returned failed")
                # _val, _type = printValue(ctlVal)
                # print(_val, _type)
                lastApplError = lib61850.ControlObjectClient_getLastApplError(
                    control)
                addCause = AddCause(lastApplError.addCause).name
                print("LastApplError: %i, addCause: %s" %
                            (lastApplError.error, addCause))
            
            time.sleep(1)

            lib61850.MmsValue_delete(ctlVal)
        
        lib61850.ControlObjectClient_destroy(control);
            
        lib61850.IedConnection_destroy(con)
            
        
