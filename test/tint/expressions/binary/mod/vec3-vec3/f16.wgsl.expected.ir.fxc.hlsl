SKIP: FAILED


[numthreads(1, 1, 1)]
void f() {
  vector<float16_t, 3> a = vector<float16_t, 3>(float16_t(1.0h), float16_t(2.0h), float16_t(3.0h));
  vector<float16_t, 3> b = vector<float16_t, 3>(float16_t(4.0h), float16_t(5.0h), float16_t(6.0h));
  vector<float16_t, 3> v = (a / b);
  vector<float16_t, 3> v_1 = floor(v);
  vector<float16_t, 3> r = ((a - (((v < (float16_t(0.0h)).xxx)) ? (ceil(v)) : (v_1))) * b);
}

FXC validation failure:
c:\src\dawn\Shader@0x000001FC4D7F4BA0(4,10-18): error X3000: syntax error: unexpected token 'float16_t'
c:\src\dawn\Shader@0x000001FC4D7F4BA0(5,10-18): error X3000: syntax error: unexpected token 'float16_t'
c:\src\dawn\Shader@0x000001FC4D7F4BA0(6,10-18): error X3000: syntax error: unexpected token 'float16_t'
c:\src\dawn\Shader@0x000001FC4D7F4BA0(7,10-18): error X3000: syntax error: unexpected token 'float16_t'
c:\src\dawn\Shader@0x000001FC4D7F4BA0(8,10-18): error X3000: syntax error: unexpected token 'float16_t'

