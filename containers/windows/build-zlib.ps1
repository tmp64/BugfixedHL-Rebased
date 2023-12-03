. ./_common.ps1

$ImageTag = "bhl-build-zlib"
$TempCont = "${ImageTag}-temp"
$DockerFile = "DockerFile.$ImageTag"
$OutDir="out/zlib"

rm -r -fo -ea 0 $OutDir
mkdir out -ea 0

Invoke-Call -ScriptBlock { docker build -f $DockerFile -t $ImageTag . }
Invoke-Call -ScriptBlock { docker container create --name $TempCont $ImageTag }
Invoke-Call -ScriptBlock { docker container cp ${TempCont}:C:/bhl/prefix-out $OutDir }
Invoke-Call -ScriptBlock { docker container rm ${TempCont} }
