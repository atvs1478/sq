#!/usr/bin/env python
#
# Wrapper to run make and preprocess any paths in the output from MSYS Unix-style paths
# to Windows paths, for Eclipse
from __future__ import print_function, division
import sys
import subprocess
import os.path
import os
import re
import glob
from test import test_cmd_line

#UNIX_PATH_RE = re.compile(r'(([a-zA-Z]{1}[:]{1}){0,1}[/\\][^\s\'\"\t\[\(]+)+')
UNIX_PATH_RE = re.compile(r'(([a-zA-Z]{1}[:]{1}){0,1}[/\\][^\s\'\"\t\[\(]+(?![^\\/\n]*$)[/\\]?)')
INCLUDE_PATH_RE = re.compile(r'-I[\s"]{0,}(.+?)["]{0,}(?=\s-\S)')
INCLUDE_PATH_ADJ_RE = re.compile(r'^([/]opt[/]esp-idf[/]){1}(.*)')
INCLUDE_PATH_ADJ2_RE = re.compile(r'^([/]c[/]){1}(.*)')

paths = {}
names = []
idf_path= os.environ.get('IDF_PATH').replace("/", "\\")
cwd_path= os.environ.get('CWD')
pwd_path= os.environ.get('PWD')         
         
def check_path(path):
    try:
        return paths[path]
    except KeyError:
        pass
    paths[path] = path
    winpath =path
    
    if not os.path.exists(winpath):
          # cache as failed, replace with success if it works
        if re.match(INCLUDE_PATH_ADJ2_RE, path) is not None:
                winpath = INCLUDE_PATH_ADJ2_RE.sub(r'c:/\2',path) #replace /c/ 
        try:
            winpath = subprocess.check_output(["cygpath", "-w", winpath]).strip()
        except subprocess.CalledProcessError:
            return path  # something went wrong running cygpath, assume this is not a path!
    if not os.path.exists(winpath):
        if not os.path.exists(winpath):
            winpath=idf_path  + '\\' + re.sub(r'^[/\\]opt[/\\](esp-idf[/\\]){0,}', '', path, 1)
            try:
                winpath = subprocess.check_output(["cygpath", "-w", winpath]).strip()
            except subprocess.CalledProcessError:
                return path  # something went wrong running cygpath, assume this is not a path!
            if not os.path.exists(winpath):
                return path  # not actually a valid path
    
    winpath = winpath.replace("/", "\\")  # make consistent with forward-slashes used elsewhere
    paths[path] = winpath
    
    
    #print("In path: {0}, out path: {1}".format(path,winpath) )
    return winpath
def fix_paths(filename):
    if re.match(r'.*[\\](.*$)',filename) is not None:
        filename = re.findall(r'.*[\\](.*$)',filename)[0].replace("\\", "/")
    
    return filename.rstrip()

def print_paths(path_list, file_name, source_file):
    
    new_path_list = list(set(path_list))
    new_path_list.sort()
    last_n = ''
    cmd_line='xtensa-esp32-elf-gcc '
    for n in new_path_list:
        if re.match(INCLUDE_PATH_ADJ_RE, n) is not None:
            n = INCLUDE_PATH_ADJ_RE.sub(idf_path+r"\2",n )
        if re.match(INCLUDE_PATH_ADJ2_RE, n) is not None:
            n = INCLUDE_PATH_ADJ2_RE.sub(r'c:/\2',n)
        if last_n != n:
            cmd_line = cmd_line + ' -I ' + n.rstrip()
        last_n = n
    if source_file:
        cmd_line = cmd_line + ' -c ' + fix_paths(source_file)
    cmd_line = cmd_line + ' -o ' + fix_paths(file_name)
    print(cmd_line)
def extract_includes():
    
    for filename in [y for x in os.walk('build') for y in glob.glob(os.path.join(x[0], '*.d'))]:
        lines = []
        source=''
        with open(filename) as file_in:
            for line in file_in:
                if re.match(r'\S*(?=/[^/]*\.[h][p]?)',line) is not None:
                    lines.extend(re.findall(r'\S*(?=/[^/]*\.[h][p]?)/',line))
                if re.match(r'\S*(?=\.[cC][pP]{0,})[^\\\s]*',line) is not None:
                    source = re.findall(r'\S*(?=\.[cC][pP]{0,})[^\\\s]*',line)[0]
        
        print_paths(lines,filename,source )
            
def main():
    cwd_path=check_path(os.getcwd())
    os.environ['CWD']= cwd_path
    os.environ['PWD']= cwd_path
    idf_path= os.environ.get('IDF_PATH').replace("/", "\\")
    cwd_path= os.environ.get('CWD')
    pwd_path= os.environ.get('PWD')
    print('Running custom script make in {}, IDF_PATH={}, CWD={}, PWD={}'.format(cwd_path,idf_path,cwd_path,pwd_path))
    
    make = subprocess.Popen(["make"] + sys.argv[1:] + ["BATCH_BUILD=1"], stdout=subprocess.PIPE)
    for line in iter(make.stdout.readline, ''):
        line = re.sub(UNIX_PATH_RE, lambda m: check_path(m.group(0)), line)
        names.extend(INCLUDE_PATH_RE.findall(line))
        print(line.rstrip())
    sys.exit(make.wait())

if __name__ == "__main__":
    main()
