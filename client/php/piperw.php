<?php
/**
 * Created by PhpStorm.
 * User: hl0rey
 * Date: 2019/8/30
 * Time: 14:10
 */

function readpipe($name){

    $name="\\\\.\\pipe\\".$name;
    $fp=fopen($name,"rb");
    //分两次读
    $len=fread($fp,4);
    $len=unpack("v",$len)[1];
    $data=fread($fp,$len);
    fclose($fp);
    echo $data;

    return $data;
}

function writepipe($name){

    $name="\\\\.\\pipe\\".$name;
    $fp=fopen($name,"wb");
    $data=file_get_contents("php://input");
    //一次性写
    fwrite($fp,$data);
    fclose($fp);


}


if(isset($_GET['action'])){

    //根据请求参数进行不同的操作
    if ($_GET['action']=='read'){
        readpipe("hlread");
    }elseif ($_GET['action']=='write'){
        writepipe("hlwrite");
    }

}else{
    //脚本执行成功
    echo "OK";
}