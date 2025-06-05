import scl_loader
import json
import os
from pathlib import Path

class IED_PARSING:
    def __init__(self, filePath):
        self.LDFilter = "ALL"
        self.filePath = filePath
        self.scd = scl_loader.SCD_handler(self.filePath, False)

        self.iedName = self.scd.get_IED_names_list()
        self.iedName = self.iedName[0]
        # print(self.iedName)
        self.ied = self.scd.get_IED_by_name(self.iedName)
        self.ip = self.scd.get_IP_Adr(self.iedName)
        self.ap = self.ip[1]
        self.ip = self.ip[0]
        # print(self.ip)
        # self.LNobj = self.get_LN_obj()
        self.dict_control_obj = {"iedName":self.iedName, "localIP":self.ip, "control":[]}
        da_list = self.ied.get_DA_leaf_nodes()

        buff_control_obj = []
        for da in da_list.values():
            if da.name == 'ctlModel' and hasattr(da, 'Val'):
                # print(" ", str(da.get_path_from_ld()).replace(".ctlModel", ""))
                # print("  ", da.Val)
                # print(self.iedName+str(da.get_path_from_ld()))
                DAraw = self.iedName+str(da.get_path_from_ld()).replace(".ctlModel", "")
                DA = DAraw.replace(".", "/", 1)
                buff_control = {"object":DA,"ctlModel": da.Val, "enabled": False}
                buff_control_obj.append(buff_control)
        self.dict_control_obj["control"] = buff_control_obj
        # print(self.dict_control_obj) 

    def saveToFileJson(self, filePath):
        buffJson = json.dumps(self.dict_control_obj,indent=2)
        f = open(filePath, "w")
        f.write(buffJson)
        f.close()               


    
path_to_test = '/home/ubuntu/2025proj/GIPAT_SCADA/IEC61850/control/python/scl_parser'

rootdir = path_to_test
extensions = ('.icd', '.cid', '.wmv')

path = '/home/ubuntu/2025proj/GIPAT_SCADA/IEC61850/control/python/scl_parser/TEMPLATE.icd'
PARSE = IED_PARSING(path)
PARSE.saveToFileJson('/home/ubuntu/2025proj/GIPAT_SCADA/IEC61850/control/python/scl_parser/'+PARSE.iedName+".json")
# for subdir, dirs, files in os.walk(rootdir):
#     for file in files:
#         ext = os.path.splitext(file)[-1].lower()
#         if ext in extensions:
#             newPath = os.path.join(subdir, file)
#             print (newPath)

#             PARSE = IED_PARSING(newPath)
#             PARSE.saveToFileJson(subdir+"/"+PARSE.iedName+".json")