Import("env")

import os

def get_value(line):
    return line.split("=")[1].lstrip('"').rstrip().rstrip('"')

if os.path.exists(".env") and os.path.isfile(".env"):
    print("loading dotenv file")
    env_file = open(".env")
    lines = env_file.readlines()
    for line in lines:
        if line.startswith("BUTTON_NORMAL_STATE_OPEN"):
            print("swapping normal state for button")
            env.ProcessFlags("-DBUTTON_NORMAL_OPEN")
