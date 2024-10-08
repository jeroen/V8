async function run_test(){
    const {foo, bar} = await import("./modules/my-module.mjs");
    return foo + bar();
}

async function test_bad_path(){
    const {foo, bar} = await import("modules/my-module.mjs");
    return foo + bar();
}

async function test_syntax_error1(){
    const {foo, bar} = await import("./modules/broken-module.mjs");
    return foo + bar();
}
