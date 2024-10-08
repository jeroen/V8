// my-module.js
import { a,b } from './modules/my-dependency.mjs';
export const foo = a;
export function bar(){
  return b;
}
