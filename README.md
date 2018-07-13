# sqpack
[![Build Status](https://travis-ci.org/huskyproject/sqpack.svg?branch=master)](https://travis-ci.org/huskyproject/sqpack)
[![Build status](https://ci.appveyor.com/api/projects/status/3fk8040p2w4svsyg/branch/master?svg=true)](https://ci.appveyor.com/project/dukelsky/sqpack/branch/master)


**sqpack** - purges squish or jam msgbases taken from fidoconfig

## SYNTAX
```
sqpack <areamask>
```
from script: 
```
sqpack "*"
```
## DESCRIPTION
sqPack  takes  the information about squish and jam msgbases from fido‐
config and purges the msgbases according to the -m and  -p  statements.
See fidoconfig.

## BUGS
hint:  if there is a core files after first tries to run sqpack - don't
worry!  Just edit /etc/fido/config. 99% of troubles come from  a  wrong
config file.  After that  please call tparser to check your config.

## AUTHOR
Matthias Tichy (mtt@tichy.de).
