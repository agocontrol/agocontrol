#!/usr/bin/env bash
set -e 

SOURCEFILE=$(realpath $1)
OUTFILE=$2

mkdir -p $(dirname $OUTFILE)
intltool-merge -q -x html/po ${SOURCEFILE} ${OUTFILE}

# Remove first line: <?xml version...
# sed -i is not POSIX, and behaves differently on BSD vs GNU
if [ `uname -s` == "FreeBSD" ]; then
	sed -i '' '1d' ${OUTFILE}
else
	sed -i '1d' ${OUTFILE}
fi

# Restore knockout containerless control flow
# And replace all "and" and "or" respectively by "&&" and "||". Only 3 substitutions are supported !
TMPFILE="${OUTFILE}.tmp"
sed -e '/<ko opts=".*">/{s/\(.*\) and \(.*\)/\1 \&\& \2/gi
	}' \
	 -e '/<ko opts=".*">/{s/\(.*\) and \(.*\)/\1 \&\& \2/gi
	}' \
	 -e '/<ko opts=".*">/{s/\(.*\) and \(.*\)/\1 \&\& \2/gi
	}' $2 > $TMPFILE && mv $TMPFILE ${OUTFILE}

sed -e '/<ko opts=".*">/{s/\(.*\) or \(.*\)/\1 \|\| \2/gi
	}' \
    -e '/<ko opts=".*">/{s/\(.*\) or \(.*\)/\1 \|\| \2/gi
	}' \
    -e '/<ko opts=".*">/{s/\(.*\) or \(.*\)/\1 \|\| \2/gi
	}' $2 > $TMPFILE && mv $TMPFILE ${OUTFILE}

sed -e "s/<ko opts=\"\(.*\)\">/<\!\-\- ko \1 \-\->/g" ${OUTFILE} > $TMPFILE && mv $TMPFILE ${OUTFILE}
sed -e "s/<\/ko>/<\!\-\- \/ko \-\->/g" ${OUTFILE} > $TMPFILE && mv $TMPFILE ${OUTFILE}
