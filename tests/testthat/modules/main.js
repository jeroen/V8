async function run_test(){
    const {foo, bar} = await import("./modules/my-module.mjs");
    return foo + bar();
}
