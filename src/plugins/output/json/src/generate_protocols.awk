#!/bin/awk -f

BEGIN {
    print "const char *protocols[] = {"; 
    currentVal = 0;
    maxNum = 255;
}


!/^#/ {
    while(currentVal < $2) {
        print "\""currentVal"\",";
        currentVal++;
    }

	# one name per protocol number is enough
	if (currentVal == $2) {
		print "\""$3"\",";
		currentVal++;
	}
}

END {
    while (currentVal <= maxNum) {
        print "\""currentVal"\",";
        currentVal++;
    }
    print "};";
}
