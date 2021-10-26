#!/usr/bin/python3

import sys
import os
import argparse
import pathlib
import json

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description="merge plugin result into a single file ")
	parser.add_argument("-i", "--input", dest="input", type=pathlib.Path, help="input directory")
	parser.add_argument("-o", "--output", dest="output", type=pathlib.Path, help="output file path")
	args = parser.parse_args()

	if os.path.isdir(args.input) :

		meth_list = []
		un_diractable_sel = set()
		un_diractable_meth = set()
		dir_path = args.input
		for json_file in os.listdir(dir_path):
			abs_path = os.path.join(dir_path, json_file)
			if abs_path.endswith(".json") == False:
				continue
			with open(abs_path, "r") as f:
				data = f.read()
			json_obj = json.loads(data)
			meth_list += json_obj["meths"]
			un_diractable_sel.update(json_obj["sels"])
			un_diractable_meth.update(json_obj["undirect_meths"])

		local_map_entry = {}
		
		for meth in meth_list:
			if meth["sel"] in un_diractable_sel:
				continue
			if meth["name"] in un_diractable_meth:
				continue
			local_map_entry[meth["loc"]] = meth
		print(len(local_map_entry))
		merged_data = json.dumps(local_map_entry, indent=4)
		with open(args.output, "w") as f:
			f.write(merged_data)
	else:
		print("😡 There is no input directory to merge :(")

