. ./_common.ps1

$CurDir = "containers/windows"

$ImageTag = "bhl-build-bhl"
$TempCont = "${ImageTag}-temp"
$DockerFile = "Dockerfile.windows"
$OutDir = "$CurDir/out/bhl"

cd ../..
rm -r -fo -ea 0 $OutDir
mkdir $OutDir -ea 0

Invoke-Call -ScriptBlock { docker build -f $DockerFile -t $ImageTag --build-arg BHL_VER_TAG . }
Invoke-Call -ScriptBlock { docker container create --name $TempCont $ImageTag }
Invoke-Call -ScriptBlock { docker container cp ${TempCont}:C:/build/bhl/_build_out_client $OutDir }
Invoke-Call -ScriptBlock { docker container cp ${TempCont}:C:/build/bhl/_build_out_server $OutDir }
Invoke-Call -ScriptBlock { docker container rm ${TempCont} }
