import sys
hdr = open('C:/Users/littl/github/gta-reversed/tools/standalone/gameplay_regress/routes/header_free.inc', encoding='utf-8').read()
out=[hdr,'cmd:tp:2495:-1688:13.5','until:cutscene:60000','wait:9000','mark:skip1','key:return','wait:35000','mark:end','cmd:quit','wait:5000']
open(sys.argv[1],'w',newline='\n').write('\n'.join(out)+'\n')
