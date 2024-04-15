# pip3 install g2p_en
from g2p_en import G2p
import argparse

# python3 gen_sr_commands.py "Turn on the light,Switch on the light;Turn off the light,Switch off the light,Go dark;\
# Start fan;Stop fan;Volume down,Turn down;Mute sound;Next song;Pause playback"
# enum {
#   SR_CMD_TURN_ON_THE_LIGHT,
#   SR_CMD_TURN_OFF_THE_LIGHT,
#   SR_CMD_START_FAN,
#   SR_CMD_STOP_FAN,
#   SR_CMD_VOLUME_DOWN,
#   SR_CMD_MUTE_SOUND,
#   SR_CMD_NEXT_SONG,
#   SR_CMD_PAUSE_PLAYBACK,
# };
# static const sr_cmd_t sr_commands[] = {
#   { 0, "Turn on the light", "TkN nN jc LiT"},
#   { 0, "Switch on the light", "SWgp nN jc LiT"},
#   { 1, "Turn off the light", "TkN eF jc LiT"},
#   { 1, "Switch off the light", "SWgp eF jc LiT"},
#   { 1, "Go dark", "Gb DnRK"},
#   { 2, "Start fan", "STnRT FaN"},
#   { 3, "Stop fan", "STnP FaN"},
#   { 4, "Volume down", "VnLYoM DtN"},
#   { 4, "Turn down", "TkN DtN"},
#   { 5, "Mute sound", "MYoT StND"},
#   { 6, "Next song", "NfKST Sel"},
#   { 7, "Pause playback", "PeZ PLdBaK"},
# };


def english_g2p(text):
    g2p = G2p()
    out = "static const sr_cmd_t sr_commands[] = {\n"
    enum = "enum {\n"
    alphabet = {
        "AE1": "a",
        "N": "N",
        " ": " ",
        "OW1": "b",
        "V": "V",
        "AH0": "c",
        "L": "L",
        "F": "F",
        "EY1": "d",
        "S": "S",
        "B": "B",
        "R": "R",
        "AO1": "e",
        "D": "D",
        "AH1": "c",
        "EH1": "f",
        "OW0": "b",
        "IH0": "g",
        "G": "G",
        "HH": "h",
        "K": "K",
        "IH1": "g",
        "W": "W",
        "AY1": "i",
        "T": "T",
        "M": "M",
        "Z": "Z",
        "DH": "j",
        "ER0": "k",
        "P": "P",
        "NG": "l",
        "IY1": "m",
        "AA1": "n",
        "Y": "Y",
        "UW1": "o",
        "IY0": "m",
        "EH2": "f",
        "CH": "p",
        "AE0": "a",
        "JH": "q",
        "ZH": "r",
        "AA2": "n",
        "SH": "s",
        "AW1": "t",
        "OY1": "u",
        "AW2": "t",
        "IH2": "g",
        "AE2": "a",
        "EY2": "d",
        "ER1": "k",
        "TH": "v",
        "UH1": "w",
        "UW2": "o",
        "OW2": "b",
        "AY2": "i",
        "UW0": "o",
        "AH2": "c",
        "EH0": "f",
        "AW0": "t",
        "AO2": "e",
        "AO0": "e",
        "UH0": "w",
        "UH2": "w",
        "AA0": "n",
        "AY0": "i",
        "IY2": "m",
        "EY0": "d",
        "ER2": "k",
        "OY2": "u",
        "OY0": "u",
    }

    cmd_id = 0
    phrase_id = 0
    text_list = text.split(";")
    for item in text_list:
        item = item.split(",")
        phrase_id = 0
        for phrase in item:
            labels = g2p(phrase)
            phoneme = ""
            for char in labels:
                if char not in alphabet:
                    print("skip %s, not found in alphabet")
                    continue
                else:
                    phoneme += alphabet[char]
            out += "  { " + str(cmd_id) + ', "' + phrase + '", "' + phoneme + '"},\n'
            if phrase_id == 0:
                enum += "  SR_CMD_" + phrase.upper().replace(" ", "_") + ",\n"
            phrase_id += 1
        cmd_id += 1
    out += "};"
    enum += "};"
    # print(text)
    print(enum)
    print(out)

    return out


if __name__ == "__main__":

    parser = argparse.ArgumentParser(prog="English Speech Commands G2P")
    parser.add_argument("text", type=str, default=None, help="input text")
    args = parser.parse_args()

    if args.text is not None:
        english_g2p(args.text)
